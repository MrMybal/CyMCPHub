#include "mcp_actions.hpp"

namespace hub {
McpActions::McpActions(Server server, Options options, std::shared_ptr<Journal> journal)
    : server_(std::move(server)), options_(std::move(options)), journal_(std::move(journal)),
      performance_(journal_ ? std::make_shared<cymcp::PerformanceMonitor>() : nullptr),
      label_(server_.name + " / " + server_.source) {}

void McpActions::record_http() noexcept {
    if (!performance_) return;
    try {
        for (const auto& event : performance_->drain()) {
            if (event.at("stage") != "mcp.http") continue;
            const auto status = event.at("status").get<std::string>();
            const auto detail = std::to_string(event.at("output_bytes").get<std::size_t>()) + " bytes";
            journal_->add(label_, event.at("name").get<std::string>(), "HTTP " + status, detail, event.at("elapsed_ms").get<double>());
        }
    } catch (...) { }
}

Json McpActions::request(const char* operation, const cymcp::Cancellation& input, std::chrono::seconds timeout,
                         const std::function<Json(cymcp::McpClient&, const cymcp::Cancellation&)>& action) {
    std::lock_guard lock(mutex_);
    input.check();
    const char* phase = "connect";
    auto started = cymcp::Clock::now();
    const auto elapsed = [&] { return std::chrono::duration<double, std::milli>(cymcp::Clock::now() - started).count(); };
    try {
        if (!client_) {
            auto connecting = input.child();
            connecting.deadline = std::min(connecting.deadline, cymcp::Clock::now() + std::chrono::seconds(20));
            cymcp::McpClientOptions client_options;
            client_options.performance = performance_;
            auto client = make_mcp_client(server_, options_, std::move(client_options));
            client->connect(connecting);
            client_ = std::move(client);
            record_http();
            if (journal_) journal_->add(label_, phase, "OK", server_.transport, elapsed());
        }
        phase = operation;
        started = cymcp::Clock::now();
        auto call = input.child();
        call.deadline = std::min(call.deadline, started + timeout);
        auto result = action(*client_, call);
        record_http();
        const bool tool_error = result.is_object() && result.value("isError", Json()) == true;
        if (journal_) journal_->add(label_, phase, tool_error ? "ToolError" : "OK", server_.transport, elapsed());
        return result;
    } catch (const cymcp::Error& error) {
        record_http();
        if (journal_) journal_->add(label_, phase, error.code, error.what(), elapsed());
        if (error.code == "InvalidState" || error.code == "UnexpectedEOF" || error.code == "ProcessClosed") client_.reset();
        throw;
    } catch (const std::exception& error) {
        record_http();
        if (journal_) journal_->add(label_, phase, "Error", error.what(), elapsed());
        throw;
    }
}

Json McpActions::tools(const cymcp::Cancellation& cancellation) {
    return request("tools/list", cancellation, std::chrono::seconds(45), [](auto& client, const auto& cancel) {
        client.clear_cache();
        return client.list_tools(cancel);
    });
}

Json McpActions::call(const std::string& name, const Json& args, const cymcp::Cancellation& cancellation) {
    return request("tools/call", cancellation, std::chrono::minutes(3), [&](auto& client, const auto& cancel) {
        return client.call_tool(name, args, cancel);
    });
}

Json McpActions::resources(const cymcp::Cancellation& cancellation) {
    return request("resources/list", cancellation, std::chrono::seconds(45), [](auto& client, const auto& cancel) {
        return client.list_resources(cancel);
    });
}

Json McpActions::read(const std::string& uri, const cymcp::Cancellation& cancellation) {
    return request("resources/read", cancellation, std::chrono::seconds(45), [&](auto& client, const auto& cancel) {
        return client.read_resource(uri, cancel);
    });
}
} // namespace hub
