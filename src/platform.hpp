// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "catalog.hpp"
#include <ctime>

namespace hub {
struct Process {
    unsigned long pid;
    std::vector<std::string> arguments;
};

struct Listener {
    std::string url;
    unsigned long pid;
};

fs::path from_utf8(std::string_view value);
fs::path environment_path(const char* name);
fs::path home_path();
fs::path client_config_path();
fs::path data_path();
fs::path executable_path();
bool same_path(const fs::path& left, const fs::path& right);
bool path_is_link(const fs::path& path);
unsigned long process_id();
std::vector<Process> running_processes(const cymcp::Cancellation& cancellation, bool& available);
std::vector<Listener> local_listeners(bool& available);
std::string local_time(std::time_t time);
bool french_locale();

class FileLock {
    std::intptr_t handle_;
public:
    explicit FileLock(const fs::path& path);
    ~FileLock();
    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;
};

std::error_code publish_file(const fs::path& temporary, const fs::path& destination, bool replace);
} // namespace hub
