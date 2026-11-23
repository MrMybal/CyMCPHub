// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "journal.hpp"

namespace hub {
void Journal::add(std::string_view source, std::string_view operation, std::string_view status,
                  std::string_view detail, double elapsed_ms) noexcept {
    try {
        const auto time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        JournalEntry entry{time, cymcp::redact_error_message(source), cymcp::redact_error_message(operation),
                           cymcp::redact_error_message(status), cymcp::redact_error_message(detail), elapsed_ms};
        std::lock_guard lock(mutex_);
        entries_.push_back(std::move(entry));
        if (entries_.size() > 512) entries_.pop_front();
    } catch (...) { }
}

std::vector<JournalEntry> Journal::entries() const {
    std::lock_guard lock(mutex_);
    return {entries_.begin(), entries_.end()};
}

void Journal::clear() noexcept {
    std::lock_guard lock(mutex_);
    entries_.clear();
}

std::string Journal::json_lines() const {
    std::string output;
    for (const auto& entry : entries()) {
        output += cymcp::Json({{"timestamp_ms", entry.timestamp_ms}, {"source", entry.source}, {"operation", entry.operation},
                               {"status", entry.status}, {"detail", entry.detail}, {"elapsed_ms", entry.elapsed_ms}}).dump();
        output += '\n';
    }
    return output;
}
} // namespace hub
