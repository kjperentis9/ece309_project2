#include "core/conversation.h"
#include "core/message.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include "core/sentinel_scanner.h"
#include "harness/harness.h"
#include "model/scripted_client.h"

#include <memory>
#include <sstream>

#include "model/replay_client.h"
#include <fstream>

// Check the default and custom Message constructors.
void test_message() {
    Message empty;
    assert(empty.role() == Role::System);
    assert(empty.content() == "");

    Message message(Role::User, "hello");
    assert(message.role() == Role::User);
    assert(message.content() == "hello");
}

// Check an empty conversation and invalid access.
void test_empty() {
    Conversation conversation;

    assert(conversation.size() == 0);
    assert(conversation.begin() == conversation.end());

    bool caught = false;

    try {
        conversation.at(0);
    } catch (const std::out_of_range&) {
        caught = true;
    }

    assert(caught);
}

// Allow tests to inspect the array's capacity.
struct ConversationTestAccess {
    static std::size_t capacity(const Conversation& conversation) {
        return conversation.capacity_;
    }
};

// Check exact capacity growth and preservation of messages.
void test_growth() {
    Conversation conversation;

    assert(ConversationTestAccess::capacity(conversation) == 0);

    conversation.append(Message(Role::System, "Be concise."));

    std::size_t expected_capacity = 1;

    assert(ConversationTestAccess::capacity(conversation) == 1);

    for (int i = 0; i < 100; i++) {
        if (conversation.size() == expected_capacity) {
            expected_capacity *= 2;
        }

        conversation.append(Message(Role::User, std::to_string(i)));

        assert(conversation.size() == static_cast<std::size_t>(i + 2));
        assert(ConversationTestAccess::capacity(conversation)
               == expected_capacity);
    }

    assert(conversation.size() == 101);
    assert(ConversationTestAccess::capacity(conversation) == 128);

    assert(conversation.at(0).role() == Role::System);
    assert(conversation.at(0).content() == "Be concise.");

    for (int i = 0; i < 100; i++) {
        assert(conversation.at(i + 1).content() == std::to_string(i));
    }

    bool caught = false;

    try {
        conversation.at(conversation.size());
    } catch (const std::out_of_range&) {
        caught = true;
    }

    assert(caught);
}

// Check iteration order.
void test_iteration() {
    Conversation conversation;

    conversation.append(Message(Role::User, "hello"));
    conversation.append(Message(Role::Assistant, "hi"));

    std::string combined;
    int count = 0;

    for (const Message& message : conversation) {
        combined += message.content();
        count++;
    }

    assert(combined == "hellohi");
    assert(count == 2);
}

// Check that the copy constructor creates independent storage.
void test_copy_constructor() {
    Conversation original;
    original.append(Message(Role::User, "hello"));

    Conversation copy(original);

    assert(copy.begin() != original.begin());
    assert(copy.size() == 1);
    assert(copy.at(0).content() == "hello");

    original = Conversation();

    assert(copy.at(0).content() == "hello");
}

// Check copying into a conversation that already owns an array.
void test_copy_assignment() {
    Conversation original;
    original.append(Message(Role::User, "new message"));

    Conversation copy;
    copy.append(Message(Role::Assistant, "old message"));

    copy = original;

    assert(copy.begin() != original.begin());
    assert(copy.size() == 1);
    assert(copy.at(0).content() == "new message");

    copy = copy;

    assert(copy.size() == 1);
    assert(copy.at(0).content() == "new message");

    Conversation empty;
    copy = empty;

    assert(copy.size() == 0);
    assert(copy.begin() == copy.end());
}

// Check that the move constructor transfers the existing array.
void test_move_constructor() {
    Conversation original;
    original.append(Message(Role::User, "hello"));

    const Message* old_address = original.begin();

    Conversation moved(std::move(original));

    assert(moved.begin() == old_address);
    assert(moved.at(0).content() == "hello");
    assert(original.size() == 0);
    assert(original.begin() == nullptr);
    assert(original.begin() == original.end());

    original.append(Message(Role::User, "reused"));

    assert(original.at(0).content() == "reused");
    assert(moved.at(0).content() == "hello");
}

// Check moving into a conversation that already owns an array.
void test_move_assignment() {
    Conversation original;
    original.append(Message(Role::User, "new message"));

    const Message* old_address = original.begin();

    Conversation moved;
    moved.append(Message(Role::Assistant, "old message"));

    moved = std::move(original);

    assert(moved.begin() == old_address);
    assert(moved.size() == 1);
    assert(moved.at(0).content() == "new message");
    assert(original.size() == 0);
    assert(original.begin() == nullptr);

    original.append(Message(Role::User, "reused"));
    assert(original.at(0).content() == "reused");
}

// Allow tests to inspect the private pending buffer.
struct ScannerTestAccess {
    static std::size_t pending_size(const SentinelScanner& scanner) {
        return scanner.pending_.size();
    }
};

// Ordinary text must come back unchanged.
void test_scanner_clean_text() {
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

// A full sentinel stops output and discards anything after it.
void test_scanner_whole_sentinel() {
    SentinelScanner scanner("<|end_conversation|>");

    auto result = scanner.feed(
        "Goodbye.<|end_conversation|>Do not print this");

    assert(result.safe_text == "Goodbye.");
    assert(result.sentinel_found);

    auto extra = scanner.feed("More text");
    assert(extra.safe_text.empty());
    assert(extra.sentinel_found);

    assert(scanner.flush().safe_text.empty());
}

// Try every possible split into two chunks.
void test_scanner_every_split() {
    std::string text = "Goodbye.<|end_conversation|>";

    for (std::size_t split = 0; split <= text.size(); split++) {
        SentinelScanner scanner("<|end_conversation|>");

        auto first = scanner.feed(text.substr(0, split));
        auto second = scanner.feed(text.substr(split));

        assert(first.sentinel_found || second.sentinel_found);
        assert(first.safe_text + second.safe_text == "Goodbye.");
        assert(scanner.flush().safe_text.empty());
    }
}

// Detect a sentinel delivered one character at a time.
void test_scanner_one_character() {
    SentinelScanner scanner("<|end_conversation|>");
    std::string text = "Bye.<|end_conversation|>";
    std::string output;

    for (std::size_t i = 0; i < text.size(); i++) {
        auto result = scanner.feed(text.substr(i, 1));
        output += result.safe_text;

        if (i == text.size() - 1) {
            assert(result.sentinel_found);
        } else {
            assert(!result.sentinel_found);
        }
    }

    assert(output == "Bye.");
    assert(scanner.flush().safe_text.empty());
}

// Similar text and incomplete markers must not trigger a stop.
void test_scanner_false_alarm() {
    SentinelScanner scanner("<|end_conversation|>");
    std::string text = "Test <|end_world|> and <|end_";

    auto result = scanner.feed(text);
    auto last = scanner.flush();

    assert(!result.sentinel_found);
    assert(!last.sentinel_found);
    assert(result.safe_text + last.safe_text == text);

    assert(scanner.flush().safe_text.empty());
}

// Feed 4 MiB of repeated partial matches one byte at a time.
void test_scanner_bounded_memory() {
    std::string sentinel = "<|end_conversation|>";
    SentinelScanner scanner(sentinel);

    std::string pattern = "<|end_";
    std::size_t total = 4 * 1024 * 1024;
    std::size_t emitted = 0;

    for (std::size_t i = 0; i < total; i++) {
        std::string chunk(1, pattern[i % pattern.size()]);
        auto result = scanner.feed(chunk);

        assert(!result.sentinel_found);
        assert(ScannerTestAccess::pending_size(scanner)
               <= sentinel.size() - 1);

        // Verify each emitted character, not just the total length.
        for (char character : result.safe_text) {
            assert(character == pattern[emitted % pattern.size()]);
            emitted++;
        }
    }

    auto last = scanner.flush();
    assert(!last.sentinel_found);

    for (char character : last.safe_text) {
        assert(character == pattern[emitted % pattern.size()]);
        emitted++;
    }

    assert(emitted == total);
    assert(ScannerTestAccess::pending_size(scanner) == 0);
}

// Supply predefined input instead of waiting for keyboard input.
class TestInput : public InputSource {
public:
    TestInput(std::string text) {
        input_.str(text);
    }

    std::string read_line() override {
        std::string line;

        if (std::getline(input_, line)) {
            return line;
        }

        ended_ = true;
        return "";
    }

    bool is_eof() const override {
        return ended_;
    }

private:
    std::istringstream input_;
    bool ended_ = false;
};

// Collect output so tests can check it.
class TestOutput : public OutputSink {
public:
    std::string text;

    void write(std::string_view part) override {
        text.append(part);
    }
};

// Stop after two turns, before the third scripted response.
void test_harness_turn_limit() {
    auto model = std::make_unique<ScriptedModelClient>(
        "scripts/greeting.script");

    HarnessConfig config;
    config.max_turns = 2;
    config.system_message = model->system_message();

    Harness harness(std::move(model), config);

    TestInput input("hello\nhelp\nbye\n");
    TestOutput output;

    auto reason = harness.run(input, output);

    assert(reason.kind == StopReason::Kind::TurnLimit);
    assert(harness.conversation().size() == 5);
    assert(harness.conversation().at(0).role() == Role::System);
    assert(harness.conversation().at(0).content() == "Be concise.");
    assert(harness.conversation().at(3).content() == "help");
    assert(output.text.find("Goodbye!") == std::string::npos);
}

// Stop at the sentinel in the third scripted response.
void test_harness_sentinel() {
    auto model = std::make_unique<ScriptedModelClient>(
        "scripts/greeting.script");

    HarnessConfig config;
    config.max_turns = 10;
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

    assert(harness.conversation().at(6).role() == Role::Assistant);
    assert(harness.conversation().at(6).content()
           == "Goodbye!<|end_conversation|>");

    // The harness should not read another input after stopping.
    assert(input.read_line() == "extra");
}

// Stop gracefully when there is no more user input.
void test_harness_eof() {
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
    assert(harness.conversation().at(2).content()
           == "I am doing well, thank you! How can I help you?");
}

// Save a conversation, replay it, and compare the results.
void test_transcript_round_trip() {
    auto model = std::make_unique<ScriptedModelClient>(
        "scripts/greeting.script");

    HarnessConfig config;
    config.system_message = model->system_message();

    Harness original(std::move(model), config);

    TestInput original_input("hello\nhelp\nbye\n");
    TestOutput original_output;

    auto original_reason = original.run(original_input, original_output);
    assert(original_reason.kind == StopReason::Kind::Sentinel);

    const Conversation& saved = original.conversation();

    // Write a transcript using the format from the spec.
    std::string path = "build/p2_round_trip.txt";
    std::ofstream file(path);
    assert(file.is_open());

    for (std::size_t i = 0; i < saved.size(); i++) {
        if (i > 0) {
            file << "---\n";
        }

        const Message& message = saved.at(i);

        if (message.role() == Role::System) {
            file << "role: system\n";
        } else if (message.role() == Role::User) {
            file << "role: user\n";
        } else {
            file << "role: assistant\n";
        }

        file << message.content() << "\n";
    }

    file.close();
    assert(!file.fail());

    // Load the saved assistant responses into the replay client.
    auto replay_model = std::make_unique<ReplayModelClient>(path);

    HarnessConfig replay_config;
    replay_config.system_message = replay_model->system_message();

    Harness replay(std::move(replay_model), replay_config);

    TestInput replay_input("hello\nhelp\nbye\n");
    TestOutput replay_output;

    auto replay_reason = replay.run(replay_input, replay_output);

    // The replay must stop for the same reason and print the same text.
    assert(replay_reason.kind == original_reason.kind);
    assert(replay_reason.detail == original_reason.detail);
    assert(replay_output.text == original_output.text);

    // Every stored role and message must also match.
    const Conversation& replayed = replay.conversation();

    assert(replayed.size() == saved.size());

    for (std::size_t i = 0; i < saved.size(); i++) {
        assert(replayed.at(i).role() == saved.at(i).role());
        assert(replayed.at(i).content() == saved.at(i).content());
    }
}

int main() {
    test_message();
    test_empty();
    test_growth();
    test_iteration();
    test_copy_constructor();
    test_copy_assignment();
    test_move_constructor();
    test_move_assignment();

    test_scanner_clean_text();
    test_scanner_whole_sentinel();
    test_scanner_every_split();
    test_scanner_one_character();
    test_scanner_false_alarm();
    test_scanner_bounded_memory();

    test_harness_turn_limit();
    test_harness_sentinel();
    test_harness_eof();

    test_transcript_round_trip();

    std::cout << "All 18 tests passed.\n";

    return 0;
}
