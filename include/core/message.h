#pragma once

#include <string>

enum class Role { System, User, Assistant };

class Message {
public:
    // Default constructor: make an empty System message.
    Message() {
        role_ = Role::System;
        content_ = "";
    }

    // Constructor: store the given role and message text.
    Message(Role role, std::string content) {
        role_ = role;
        content_ = content;
    }

    // Return the role of this message.
    Role role() const noexcept {
        return role_;
    }

    // Return the text of this message.
    const std::string& content() const noexcept {
        return content_;
    }

private:
    Role role_;
    std::string content_;
};