#include "core/conversation.h"
#include <stdexcept>
#include <utility>

// Create an empty conversation.
Conversation::Conversation() {
    data_ = nullptr;
    size_ = 0;
    capacity_ = 0;
}

// Release the allocated array.
Conversation::~Conversation() {
    delete[] data_;
}

// Return the number of stored messages.
std::size_t Conversation::size() const noexcept {
    return size_;
}

// Return a message after checking the index.
const Message& Conversation::at(std::size_t i) const {
    if (i >= size_) {
        throw std::out_of_range("Message index is out of range");
    }

    return data_[i];
}

// Return a pointer to the first message.
const Message* Conversation::begin() const noexcept {
    return data_;
}

// Return a pointer just past the last stored message.
const Message* Conversation::end() const noexcept {
    if (size_ == 0) {
        return data_;
    }

    return data_ + size_;
}

// Add a message to the end of the conversation.
void Conversation::append(Message m) {
    // If the array is full, make a larger one.
    if (size_ == capacity_) {
        std::size_t new_capacity;

        if (capacity_ == 0) {
            new_capacity = 1;
        } else {
            new_capacity = capacity_ * 2;
        }

        Message* new_data = new Message[new_capacity];

        // Move existing messages into the new array.
        for (std::size_t i = 0; i < size_; i++) {
            new_data[i] = std::move(data_[i]);
        }

        // Release the old array and use the new one.
        delete[] data_;
        data_ = new_data;
        capacity_ = new_capacity;
    }

    // Put the new message in the next available slot.
    data_[size_] = std::move(m);
    size_++;
}

// Create a new conversation by copying another one.
Conversation::Conversation(const Conversation& other) {
    if (other.capacity_ == 0) {
        return;
    }

    Message* new_data = new Message[other.capacity_];

    try {
        for (std::size_t i = 0; i < other.size_; i++) {
            new_data[i] = other.data_[i];
        }
    } catch (...) {
        delete[] new_data;
        throw;
    }

    data_ = new_data;
    size_ = other.size_;
    capacity_ = other.capacity_;
}

// Replace this conversation with a copy of another one.
Conversation& Conversation::operator=(const Conversation& other) {
    if (this == &other) {
        return *this;
    }

    Conversation copy(other);

    std::swap(data_, copy.data_);
    std::swap(size_, copy.size_);
    std::swap(capacity_, copy.capacity_);

    return *this;
}

// Create a conversation by taking ownership of another's array.
Conversation::Conversation(Conversation&& other) noexcept {
    data_ = other.data_;
    size_ = other.size_;
    capacity_ = other.capacity_;

    // Leave the source empty.
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

// Replace this conversation by taking ownership of another's array.
Conversation& Conversation::operator=(Conversation&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    // Release the array this conversation currently owns.
    delete[] data_;

    data_ = other.data_;
    size_ = other.size_;
    capacity_ = other.capacity_;

    // Leave the source empty.
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;

    return *this;
}