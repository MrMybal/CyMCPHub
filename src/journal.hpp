#pragma once

#include <cymcp/core.hpp>

namespace hub {
struct JournalEntry {
    std::int64_t timestamp_ms;
    std::string source, operation, status, detail;
    double elapsed_ms;
};

class Journal {
    mutable std::mutex mutex_;
    std::deque<JournalEntry> entries_;

public:
    void add(std::string_view source, std::string_view operation, std::string_view status,
             std::string_view detail = {}, double elapsed_ms = 0) noexcept;
    std::vector<JournalEntry> entries() const;
    std::string json_lines() const;
};
} // namespace hub
