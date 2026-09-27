#include "core/sentinel_scanner.h"
#include <stdexcept>

// Store the sentinel we will search for.
SentinelScanner::SentinelScanner(std::string sentinel) {
    if (sentinel.empty()) {
        throw std::invalid_argument("Sentinel cannot be empty");
    }

    sentinel_ = sentinel;
    pending_ = "";
    stopped_ = false;
}

// Check the next chunk for the sentinel.
SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    Out result;
    result.safe_text = "";
    result.sentinel_found = false;

    // Ignore any text arriving after the sentinel.
    if (stopped_) {
        result.sentinel_found = true;
        return result;
    }

    // Combine held-back text with the new chunk.
    std::string text = pending_;
    text.append(chunk);

    std::size_t position = text.find(sentinel_);

    if (position != std::string::npos) {
        // Return only the text before the sentinel.
        result.safe_text = text.substr(0, position);
        result.sentinel_found = true;

        pending_.clear();
        stopped_ = true;

        return result;
    }

    // Keep enough trailing characters to catch a split sentinel.
    std::size_t keep = sentinel_.size() - 1;

    if (text.size() <= keep) {
        pending_ = text;
    } else {
        std::size_t safe_count = text.size() - keep;

        result.safe_text = text.substr(0, safe_count);
        pending_ = text.substr(safe_count);
    }

    return result;
}

// Release held-back text when no more chunks are coming.
SentinelScanner::Out SentinelScanner::flush() {
    Out result;

    result.safe_text = pending_;
    result.sentinel_found = stopped_;

    pending_.clear();

    return result;
}