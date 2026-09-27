#pragma once

#include <string>
#include <string_view>

class SentinelScanner {
public:
    // Set the text that signals the conversation should stop.
    explicit SentinelScanner(std::string sentinel);

    // Store the results of scanning.
    struct Out {
        std::string safe_text;
        bool sentinel_found;
    };

    // Check the next piece of incoming text.
    Out feed(std::string_view chunk);

    // Release any remaining text when the stream ends.
    Out flush();

private:
    friend struct ScannerTestAccess;
    
    std::string sentinel_;
    std::string pending_;
    bool stopped_ = false;
};