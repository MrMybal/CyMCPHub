// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "catalog.hpp"
#include "journal.hpp"
#include <cymcp/mcp.hpp>
#include <cymcp/performance.hpp>

namespace hub {
std::unique_ptr<cymcp::McpClient> make_mcp_client(const Server& server, const Options& options,
                                                cymcp::McpClientOptions client_options);

class McpActions {
    Server server_;
    Options options_;
    std::timed_mutex mutex_;
    std::unique_ptr<cymcp::McpClient> client_;
    std::shared_ptr<Journal> journal_;
    std::shared_ptr<cymcp::PerformanceMonitor> performance_;
    std::string label_;

    void record_http() noexcept;
    Json request(const char* operation, const cymcp::Cancellation&, std::chrono::seconds,
                 const std::function<Json(cymcp::McpClient&, const cymcp::Cancellation&)>&);

public:
    McpActions(Server server, Options options, std::shared_ptr<Journal> journal = {});
    Json tools(const cymcp::Cancellation&);
    Json call(const std::string& name, const Json& arguments, const cymcp::Cancellation&);
    Json resources(const cymcp::Cancellation&);
    Json read(const std::string& uri, const cymcp::Cancellation&);
};
} // namespace hub
