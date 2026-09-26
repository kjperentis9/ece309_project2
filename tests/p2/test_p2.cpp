#include "core/conversation.h"
#include "core/message.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

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

// Check that messages survive repeated array growth.
void test_growth() {
    Conversation conversation;

    conversation.append(Message(Role::System, "Be concise."));

    for (int i = 0; i < 100; i++) {
        conversation.append(Message(Role::User, std::to_string(i)));
    }

    assert(conversation.size() == 101);
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

int main() {
    test_message();
    test_empty();
    test_growth();
    test_iteration();
    test_copy_constructor();
    test_copy_assignment();
    test_move_constructor();
    test_move_assignment();

    std::cout << "All 8 initial tests passed.\n";

    return 0;
}
