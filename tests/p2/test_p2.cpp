#include "core/conversation.h"
#include "core/message.h"
#include "core/sentinel_scanner.h"
#include "harness/harness.h"
#include "model/scripted_client.h"
#include "model/replay_client.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

// Read private values required by the tests.
struct ConversationTestAccess {
    static std::size_t capacity(const Conversation& c) {
        return c.capacity_;
    }
};

struct ScannerTestAccess {
    static std::size_t pending_size(const SentinelScanner& s) {
        return s.pending_.size();
    }
};

// Give the harness test input.
class TestInput : public InputSource {
public:
    TestInput(std::string text) {
        input.str(text);
    }

    std::string read_line() override {
        std::string line;
        if (!std::getline(input, line)) {
            ended = true;
        }
        return line;
    }

    bool is_eof() const override {
        return ended;
    }

private:
    std::istringstream input;
    bool ended = false;
};

// Save the harness output for checking.
class TestOutput : public OutputSink {
public:
    std::string text;

    void write(std::string_view part) override {
        text.append(part);
    }
};

// 1. Empty conversation and bounds.
void test_empty() {
    Conversation c;
    assert(c.size() == 0);
    assert(c.begin() == c.end());

    bool caught = false;
    try {
        c.at(0);
    } catch (const std::out_of_range&) {
        caught = true;
    }
    assert(caught);
}

// 2. System message stays first as the conversation grows.
void test_system_first() {
    Conversation c;
    c.append(Message(Role::System, "Be concise."));

    for (int i = 0; i < 20; i++) {
        c.append(Message(Role::User, "hello"));
    }

    assert(c.at(0).role() == Role::System);
    assert(c.at(0).content() == "Be concise.");
}

// 3. Copy constructor and copy assignment use separate storage.
void test_copy() {
    Conversation original;
    original.append(Message(Role::User, "hello"));

    Conversation copy(original);
    Conversation assigned;
    assigned.append(Message(Role::User, "old"));
    assigned = original;

    assert(copy.begin() != original.begin());
    assert(assigned.begin() != original.begin());

    original = Conversation();

    assert(copy.size() == 1);
    assert(copy.at(0).content() == "hello");
    assert(assigned.size() == 1);
    assert(assigned.at(0).content() == "hello");
}

// 4. Moves transfer the pointer and zero the source.
void test_move() {
    Conversation original;
    original.append(Message(Role::User, "hello"));
    const Message* address = original.begin();

    Conversation moved(std::move(original));
    assert(moved.begin() == address);
    assert(original.begin() == nullptr);
    assert(original.size() == 0);
    assert(ConversationTestAccess::capacity(original) == 0);

    Conversation assigned;
    assigned.append(Message(Role::User, "old"));
    assigned = std::move(moved);

    assert(assigned.begin() == address);
    assert(assigned.size() == 1);
    assert(assigned.at(0).content() == "hello");
    assert(moved.begin() == nullptr);
    assert(moved.size() == 0);
    assert(ConversationTestAccess::capacity(moved) == 0);
}

// 5. Capacity doubles and messages survive resizing.
void test_growth() {
    Conversation c;
    std::size_t expected = 0;
    assert(ConversationTestAccess::capacity(c) == 0);

    for (std::size_t i = 0; i < 100; i++) {
        if (i == expected) {
            expected = (expected == 0) ? 1 : expected * 2;
        }

        c.append(Message(Role::User, std::to_string(i)));
        assert(c.size() == i + 1);
        assert(ConversationTestAccess::capacity(c) == expected);

        for (std::size_t j = 0; j <= i; j++) {
            assert(c.at(j).content() == std::to_string(j));
        }
    }
}

// 6. Ordinary text is preserved, including buffered text.
void test_clean_text() {
    SentinelScanner scanner("<|end_conversation|>");
    auto first = scanner.feed("Hello ");
    auto second = scanner.feed("there!");
    auto last = scanner.flush();

    assert(first.safe_text + second.safe_text + last.safe_text
           == "Hello there!");
    assert(!first.sentinel_found);
    assert(!second.sentinel_found);
    assert(!last.sentinel_found);
}

// 7. Every two-chunk split and one-character delivery.
void test_split_sentinel() {
    std::string text = "Bye.<|end_conversation|>";

    for (std::size_t split = 0; split <= text.size(); split++) {
        SentinelScanner scanner("<|end_conversation|>");
        auto first = scanner.feed(text.substr(0, split));
        auto second = scanner.feed(text.substr(split));

        assert(first.sentinel_found || second.sentinel_found);
        assert(first.safe_text + second.safe_text == "Bye.");
    }

    SentinelScanner scanner("<|end_conversation|>");
    std::string output;

    for (std::size_t i = 0; i < text.size(); i++) {
        auto result = scanner.feed(text.substr(i, 1));
        output += result.safe_text;
        assert(result.sentinel_found == (i == text.size() - 1));
    }

    assert(output == "Bye.");
    assert(scanner.flush().safe_text.empty());
}

// 8. Similar and incomplete markers do not stop output.
void test_false_alarm() {
    SentinelScanner scanner("<|end_conversation|>");
    std::string text = "Test <|end_world|> and <|end_";
    auto result = scanner.feed(text);
    auto last = scanner.flush();

    assert(!result.sentinel_found);
    assert(!last.sentinel_found);
    assert(result.safe_text + last.safe_text == text);
}

// 9. Pending memory stays bounded over a 4 MiB stream.
void test_bounded_memory() {
    std::string sentinel = "<|end_conversation|>";
    SentinelScanner scanner(sentinel);
    std::string pattern = "<|end_";
    std::size_t total = 4 * 1024 * 1024;
    std::size_t emitted = 0;

    for (std::size_t i = 0; i < total; i++) {
        auto result = scanner.feed(
            std::string(1, pattern[i % pattern.size()]));

        assert(!result.sentinel_found);
        assert(ScannerTestAccess::pending_size(scanner)
               <= sentinel.size() - 1);

        for (char ch : result.safe_text) {
            assert(ch == pattern[emitted % pattern.size()]);
            emitted++;
        }
    }

    auto last = scanner.flush();
    assert(!last.sentinel_found);

    for (char ch : last.safe_text) {
        assert(ch == pattern[emitted % pattern.size()]);
        emitted++;
    }
    assert(emitted == total);
}

// 10. Harness stops at its turn limit.
void test_turn_limit() {
    auto model = std::make_unique<ScriptedModelClient>(
        "scripts/greeting.script");
    HarnessConfig config;
    config.system_message = model->system_message();
    config.max_turns = 2;
    Harness harness(std::move(model), config);
    TestInput input("hello\nhelp\nbye\n");
    TestOutput output;

    auto reason = harness.run(input, output);

    assert(reason.kind == StopReason::Kind::TurnLimit);
    assert(harness.conversation().size() == 5);
    assert(input.read_line() == "bye");
}

// 11. Harness stops at the sentinel without printing it.
void test_sentinel_halt() {
    auto model = std::make_unique<ScriptedModelClient>(
        "scripts/greeting.script");
    HarnessConfig config;
    config.system_message = model->system_message();
    Harness harness(std::move(model), config);
    TestInput input("hello\nhelp\nbye\nextra\n");
    TestOutput output;

    auto reason = harness.run(input, output);

    assert(reason.kind == StopReason::Kind::Sentinel);
    assert(harness.conversation().size() == 7);
    assert(output.text.find("Goodbye!") != std::string::npos);
    assert(output.text.find("<|end_conversation|>")
           == std::string::npos);
    assert(harness.conversation().at(6).content()
           == "Goodbye!<|end_conversation|>");
    assert(input.read_line() == "extra");
}

// 12. Saved conversation replays identically.
void test_round_trip() {
    auto model = std::make_unique<ScriptedModelClient>(
        "scripts/greeting.script");
    HarnessConfig config;
    config.system_message = model->system_message();
    Harness original(std::move(model), config);
    TestInput input("hello\nhelp\nbye\n");
    TestOutput output;
    auto reason = original.run(input, output);
    assert(reason.kind == StopReason::Kind::Sentinel);

    const Conversation& saved = original.conversation();
    std::string path = "build/p2_round_trip.txt";
    std::ofstream file(path);
    assert(file.is_open());

    for (std::size_t i = 0; i < saved.size(); i++) {
        if (i > 0) {
            file << "---\n";
        }

        if (saved.at(i).role() == Role::System) {
            file << "role: system\n";
        } else if (saved.at(i).role() == Role::User) {
            file << "role: user\n";
        } else {
            file << "role: assistant\n";
        }
        file << saved.at(i).content() << "\n";
    }

    file.close();
    assert(!file.fail());

    auto model2 = std::make_unique<ReplayModelClient>(path);
    HarnessConfig config2;
    config2.system_message = model2->system_message();
    Harness replay(std::move(model2), config2);
    TestInput input2("hello\nhelp\nbye\n");
    TestOutput output2;
    auto reason2 = replay.run(input2, output2);

    assert(reason2.kind == reason.kind);
    assert(reason2.detail == reason.detail);
    assert(output2.text == output.text);
    assert(replay.conversation().size() == saved.size());

    for (std::size_t i = 0; i < saved.size(); i++) {
        assert(replay.conversation().at(i).role() == saved.at(i).role());
        assert(replay.conversation().at(i).content()
               == saved.at(i).content());
    }
}

// 13. EOF ends the conversation gracefully.
void test_eof() {
    auto model = std::make_unique<ScriptedModelClient>(
        "scripts/greeting.script");
    HarnessConfig config;
    config.system_message = model->system_message();
    Harness harness(std::move(model), config);
    TestInput input("hello\n");
    TestOutput output;

    auto reason = harness.run(input, output);

    assert(reason.kind == StopReason::Kind::UserExit);
    assert(harness.conversation().size() == 3);
    assert(harness.conversation().at(1).content() == "hello");
    assert(harness.conversation().at(2).role() == Role::Assistant);
}

int main() {
    test_empty();
    test_system_first();
    test_copy();
    test_move();
    test_growth();
    test_clean_text();
    test_split_sentinel();
    test_false_alarm();
    test_bounded_memory();
    test_turn_limit();
    test_sentinel_halt();
    test_round_trip();
    test_eof();

    std::cout << "All 13 tests passed.\n";
    return 0;
}