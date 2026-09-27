#pragma once

#include "core/message.h"
#include <cstddef>

class Conversation {
public:
    // Create an empty conversation.
    Conversation();

    // Release the array when the conversation is destroyed.
    ~Conversation();

    // Copy constructor and copy assignment.
    Conversation(const Conversation& other);
    Conversation& operator=(const Conversation& other);

    // Move constructor and move assignment.
    Conversation(Conversation&& other) noexcept;
    Conversation& operator=(Conversation&& other) noexcept;

    // Add a message to the end.
    void append(Message m);

    // Return the number of stored messages.
    std::size_t size() const noexcept;

    // Return the message at a specific index.
    const Message& at(std::size_t i) const;

    // Return pointers for iterating through the messages.
    const Message* begin() const noexcept;
    const Message* end() const noexcept;

private:
    friend struct ConversationTestAccess;
    
    Message* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};