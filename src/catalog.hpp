// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cymcp/core.hpp>

namespace hub {
namespace fs = std::filesystem;
using cymcp::Json;

enum class Status { verified, process_seen, configured, authentication, unverified, disabled };
const char* status_key(Status status);
bool is_active(Status status);

struct Server {
    std::string id, name, source, transport;
    fs::path source_file;
    Status status = Status::configured;
    std::string server_name, version, protocol, summary;
    unsigned long pid = 0;
    Json connection = Json::object();
    std::string note;

    Json to_json(bool details) const;
};

struct Snapshot {
    std::vector<Server> servers;
    std::vector<fs::path> unreadable_sources;
    bool processes_available = true;
    bool ports_available = true;
};

struct Options {
    fs::path workspace, extra_config;
    bool scan_ports = true;
};

struct Arguments {
    Options options;
    bool list = false;
    bool help = false;
    bool tray = false;
};

Arguments parse_arguments(const std::vector<std::string>& arguments);
Snapshot scan(const Options& options, const cymcp::Cancellation& cancellation = {});
void set_note(const std::string& id, const std::string& note);
Json read_notes();
bool matches(const Server& server, std::string_view query);
std::string group_key(std::string_view name);
} // namespace hub
