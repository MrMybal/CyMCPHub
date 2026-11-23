// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "hub_service.hpp"
#include "mcp_actions.hpp"
#include "sdk_scan.hpp"
#include "skills.hpp"

#include <algorithm>

namespace hub {
Json inventory_metadata(const Snapshot& snapshot, bool scan_ports) {
    auto sources = Json::array();
    for (const auto& path : snapshot.unreadable_sources) sources.push_back(cymcp::path_utf8(path));
    return {{"unreadableSources", sources}, {"processInventoryAvailable", snapshot.processes_available},
            {"portScanEnabled", scan_ports}, {"portInventoryAvailable", scan_ports && snapshot.ports_available}};
}

namespace {
Json object_schema(Json properties, Json required = Json::array()) {
    return {{"type", "object"}, {"properties", properties}, {"required", required}, {"additionalProperties", false}};
}

Json bounded_string(int limit) { return {{"type", "string"}, {"maxLength", limit}}; }

Json catalogue_page(const char* field, const Json& catalogue, const Json& args) {
    const auto offset = args.value("offset", std::size_t{0});
    const auto limit = args.value("limit", std::size_t{50});
    const auto total = catalogue.size();
    auto items = Json::array();
    for (std::size_t i = offset; i < total && items.size() < limit; ++i) items.push_back(catalogue[i]);
    Json result = {{field, items}, {"total", total}};
    if (offset < total && items.size() < total - offset) result["nextOffset"] = offset + items.size();
    return result;
}

struct HubState {
    Options options;
    std::shared_ptr<Journal> journal;
    Snapshot snapshot;
    std::mutex mutex;
    std::map<std::string, std::shared_ptr<McpActions>> connections;
    static constexpr auto freshness = std::chrono::seconds(10);
    cymcp::Clock::time_point updated = cymcp::Clock::time_point::min();

    HubState(Options settings, std::shared_ptr<Journal> log) : options(std::move(settings)), journal(std::move(log)) {}

    void refresh(const cymcp::Cancellation& cancellation, bool force = false) {
        cancellation.check();
        if (force || updated == cymcp::Clock::time_point::min() || cymcp::Clock::now() - updated > freshness) {
            snapshot = scan(options, cancellation, journal.get());
            updated = cymcp::Clock::now();
        }
        Json notes = Json::object();
        try { notes = read_notes(); }
        catch (const std::exception& error) { if (journal) journal->add("CyMCPHub", "annotations/read", "Error", error.what()); }
        for (auto& server : snapshot.servers) {
            const auto found = notes.find(server.id);
            server.note = found != notes.end() && found->is_string() ? found->get<std::string>() : std::string{};
        }
    }

    Server& find(const std::string& id) {
        const auto found = std::find_if(snapshot.servers.begin(), snapshot.servers.end(),
                                        [&](const Server& server) { return server.id == id; });
        if (found == snapshot.servers.end()) throw cymcp::Error("NotFound", "Run mcp_list to find server IDs");
        return *found;
    }

    std::shared_ptr<McpActions> connection(const std::string& id, const cymcp::Cancellation& cancellation) {
        std::lock_guard lock(mutex);
        if (const auto found = connections.find(id); found != connections.end()) return found->second;
        if (std::none_of(snapshot.servers.begin(), snapshot.servers.end(), [&](const Server& server) { return server.id == id; }))
            refresh(cancellation);
        return connections.emplace(id, std::make_shared<McpActions>(find(id), options, journal)).first->second;
    }
};

Json search_catalogue(HubState& state, const Json& args, const cymcp::Cancellation& cancellation) {
    const auto requested = args.value("id", std::string{});
    const auto query = group_key(args.value("query", std::string{}));
    const auto offset = args.value("offset", std::size_t{0});
    const auto limit = args.value("limit", std::size_t{15});
    const auto tool_limit = args.value("toolLimit", std::size_t{30});
    std::vector<Server> servers;
    {
        std::lock_guard lock(state.mutex);
        state.refresh(cancellation, args.value("refresh", false));
        if (requested.empty()) servers = state.snapshot.servers;
        else servers.push_back(state.find(requested));
    }
    auto budget = cancellation.child();
    budget.deadline = std::min(budget.deadline, cymcp::Clock::now() + std::chrono::seconds(requested.empty() ? 18 : 65));
    const bool markdown = args.value("format", std::string{}) == "markdown";
    const bool include_stdio = args.value("includeStdio", false);
    auto results = Json::array();
    std::string document = "# MCP catalogue\n\n";
    std::size_t total = 0;
    bool partial = false;
    for (const auto& server : servers) {
        cancellation.check();
        if (cymcp::Clock::now() >= budget.deadline) { partial = true; break; }
        const auto label = server.server_name.empty() ? server.name : server.server_name;
        const auto summary = !server.note.empty() ? server.note : !server.summary.empty() ? server.summary :
            label + " via " + server.source + " (" + server.transport + ")";
        const bool app_matches = query.empty() || matches(server, query) || group_key(summary).find(query) != std::string::npos;
        auto tools = Json::array();
        std::size_t matched_tools = 0, tool_count = 0;
        std::string status = "not_loaded";
        if (server.status != Status::disabled && (!requested.empty() || server.status == Status::verified ||
            (include_stdio && server.transport == "stdio"))) {
            try {
                auto per_connector = budget.child();
                if (requested.empty()) per_connector.deadline = std::min(budget.deadline, cymcp::Clock::now() + std::chrono::seconds(4));
                const auto catalogue = state.connection(server.id, per_connector)->tools(per_connector);
                tool_count = catalogue.size();
                status = "loaded";
                for (const auto& tool : catalogue) {
                    if (!tool.is_object() || !tool.contains("name") || !tool.at("name").is_string()) continue;
                    const auto name = tool.at("name").get<std::string>();
                    const auto descr = tool.value("description", Json());
                    const auto description = descr.is_string() ? cymcp::redact_error_message(descr.get<std::string>()) : std::string{};
                    if (!app_matches && group_key(name).find(query) == std::string::npos &&
                        group_key(description).find(query) == std::string::npos) continue;
                    if (matched_tools++ < tool_limit) tools.push_back({{"name", name}, {"description", description}});
                }
            } catch (const cymcp::Error& error) {
                cancellation.check();
                status = error.code;
            } catch (const std::exception&) {
                status = "error";
            }
        }
        if (!app_matches && tools.empty()) continue;
        if (total++ < offset || results.size() >= limit) continue;
        auto row = server.to_json(false);
        row["summary"] = summary;
        row["toolCount"] = tool_count;
        row["matchedToolCount"] = matched_tools;
        row["toolStatus"] = status;
        row["tools"] = tools;
        results.push_back(std::move(row));
        if (!markdown) continue;
        document += "## " + cymcp::redact_error_message(label) + " · " + server.source + "\n\n";
        document += summary + "\n\n";
        for (const auto& tool : tools) document += "- **" + tool.at("name").get<std::string>() + "** — " +
                                             tool.at("description").get<std::string>() + "\n";
        document += '\n';
    }
    Json result = {{"applications", results}, {"total", total}, {"partial", partial}};
    if (offset < total && results.size() < total - offset) result["nextOffset"] = offset + results.size();
    if (markdown) result["markdown"] = std::move(document);
    return result;
}
} // namespace

std::shared_ptr<cymcp::Registry> make_hub_registry(Options options, std::shared_ptr<Journal> journal) {
    auto tools = std::make_shared<cymcp::Registry>(cymcp::Policy::allow_all());
    auto state = std::make_shared<HubState>(std::move(options), std::move(journal));
    const Json output = {{"type", "object"}};
    const auto identifier = bounded_string(24);
    tools->add({"mcp_list", "List local MCP servers and observations. Verified means a successful MCP HTTP handshake. "
                           "Process seen means the configured stdio command was observed.",
               object_schema({{"query", bounded_string(200)}, {"activeOnly", {{"type", "boolean"}}},
                              {"refresh", {{"type", "boolean"}}},
                              {"offset", {{"type", "integer"}, {"minimum", 0}}},
                              {"limit", {{"type", "integer"}, {"minimum", 1}, {"maximum", 100}}}}), output, true,
               [state](const Json& args, const cymcp::CallContext& context) {
                   std::lock_guard lock(state->mutex);
                   state->refresh(context.cancellation, args.value("refresh", false));
                   const auto offset = args.value("offset", std::size_t{0});
                   const auto limit = args.value("limit", std::size_t{50});
                   const auto query = args.value("query", std::string{});
                   const auto active_only = args.value("activeOnly", false);
                   auto page = Json::array();
                   std::size_t total = 0;
                   for (const auto& server : state->snapshot.servers) {
                       if ((active_only && !hub::is_active(server.status)) || !hub::matches(server, query)) continue;
                       if (total++ >= offset && page.size() < limit) page.push_back(server.to_json(false));
                   }
                   Json result = {{"servers", page}, {"total", total}, {"inventory", inventory_metadata(state->snapshot, state->options.scan_ports)}};
                   if (offset < total && page.size() < total - offset) result["nextOffset"] = offset + page.size();
                   return result;
               }, {}});
    tools->add({"mcp_get", "Get a server's source, connection metadata and annotation. Secret values must be resolved in the original config.",
               object_schema({{"id", identifier}}, {"id"}), output, true,
               [state](const Json& args, const cymcp::CallContext& context) {
                   std::lock_guard lock(state->mutex);
                   state->refresh(context.cancellation);
                   return state->find(args.at("id")).to_json(true);
               }, {}});
    tools->add({"mcp_annotate", "Replace a server's local annotation. An empty string clears the annotation.",
               object_schema({{"id", identifier}, {"annotation", bounded_string(8192)}}, {"id", "annotation"}), output, false,
               [state](const Json& args, const cymcp::CallContext& context) {
                   std::lock_guard lock(state->mutex);
                   state->refresh(context.cancellation);
                   auto& server = state->find(args.at("id"));
                   const auto annotation = args.at("annotation").get<std::string>();
                   context.cancellation.check();
                   hub::set_note(server.id, annotation);
                   server.note = annotation;
                   return server.to_json(true);
               }, {}});
    const auto paged_connector = object_schema({{"id", identifier}, {"offset", {{"type", "integer"}, {"minimum", 0}}},
                                                  {"limit", {{"type", "integer"}, {"minimum", 1}, {"maximum", 100}}}}, {"id"});
    tools->add({"mcp_tools", "List a connector's available tools with their descriptions and JSON input schemas. "
                            "Use mcp_list to find its ID. For stdio this starts a dedicated session.",
               paged_connector, output, true, [state](const Json& args, const cymcp::CallContext& context) {
                   return catalogue_page("tools", state->connection(args.at("id"), context.cancellation)->tools(context.cancellation), args);
               }, {}});
    tools->add({"mcp_call", "Run a tool that the user requested on a specific connector. Pass the arguments described by mcp_tools. "
                           "The raw tool result, including any isError field, is returned in result.",
               object_schema({{"id", identifier}, {"tool", bounded_string(512)}, {"arguments", {{"type", "object"}}}},
                             {"id", "tool"}), output, false, [state](const Json& args, const cymcp::CallContext& context) {
                   auto result = state->connection(args.at("id"), context.cancellation)
                                     ->call(args.at("tool"), args.value("arguments", Json::object()), context.cancellation);
                   return Json{{"result", std::move(result)}};
               }, {}});
    tools->add({"mcp_resources", "List resources exposed by the selected MCP connector.", paged_connector, output, true,
               [state](const Json& args, const cymcp::CallContext& context) {
                   return catalogue_page("resources", state->connection(args.at("id"), context.cancellation)->resources(context.cancellation), args);
               }, {}});
    tools->add({"mcp_read_resource", "Read an MCP resource by URI from the selected connector.",
               object_schema({{"id", identifier}, {"uri", bounded_string(4096)}}, {"id", "uri"}), output, true,
               [state](const Json& args, const cymcp::CallContext& context) {
                   return Json{{"result", state->connection(args.at("id"), context.cancellation)->read(args.at("uri"), context.cancellation)}};
               }, {}});
    tools->add({"mcp_disconnect", "Release the Hub's session for this connector and stop any stdio process started by it.",
               object_schema({{"id", identifier}}, {"id"}), output, false,
               [state](const Json& args, const cymcp::CallContext&) {
                   const auto id = args.at("id").get<std::string>();
                   std::shared_ptr<hub::McpActions> detached;
                   {
                       std::lock_guard lock(state->mutex);
                       auto entry = state->connections.extract(id);
                       if (!entry.empty()) detached = std::move(entry.mapped());
                   }
                   return Json{{"disconnected", static_cast<bool>(detached)}};
               }, {}});
    tools->add({"mcp_journal", "Read the current Hub process diagnostics as JSON Lines. Records include HTTP status, method and latency.",
               object_schema(Json::object()), output, true,
               [state](const Json&, const cymcp::CallContext&) { return Json{{"jsonLines", state->journal->json_lines()}}; }, {}});
    tools->add({"mcp_catalog", "Search MCP applications and their tools. Return application summaries and optionally Markdown. "
                             "Verified HTTP connectors are queried automatically; specify a connector ID or includeStdio to start configured stdio servers.",
               object_schema({{"id", identifier}, {"query", bounded_string(200)},
                              {"format", {{"type", "string"}, {"enum", {"json", "markdown"}}}},
                              {"includeStdio", {{"type", "boolean"}}}, {"refresh", {{"type", "boolean"}}},
                              {"offset", {{"type", "integer"}, {"minimum", 0}}},
                              {"limit", {{"type", "integer"}, {"minimum", 1}, {"maximum", 35}}},
                              {"toolLimit", {{"type", "integer"}, {"minimum", 1}, {"maximum", 100}}}}), output, true,
               [state](const Json& args, const cymcp::CallContext& context) { return search_catalogue(*state, args, context.cancellation); }, {}});
    tools->add({"mcp_list_skills", "List collected and authored SKILL.md files in the Hub library. Filter by connector ID or query.",
               object_schema({{"id", identifier}, {"query", bounded_string(200)}}), output, true,
               [](const Json& args, const cymcp::CallContext&) {
                   const auto id = args.value("id", std::string{}), query = group_key(args.value("query", std::string{}));
                   auto skills = Json::array();
                   for (const auto& skill : list_skills()) {
                       if ((!id.empty() && id != skill.connector) || (!query.empty() && group_key(skill.name).find(query) == std::string::npos &&
                             group_key(skill.description).find(query) == std::string::npos)) continue;
                       skills.push_back(skill.to_json());
                   }
                   return Json{{"skills", skills}};
               }, {}});
    tools->add({"mcp_read_skill", "Read the Markdown of a skill collected or created in CyMCPHub.",
               object_schema({{"connectorId", identifier}, {"name", bounded_string(64)}}, {"connectorId", "name"}), output, true,
               [](const Json& args, const cymcp::CallContext&) {
                   const auto& id = args.at("connectorId").get_ref<const std::string&>();
                   const auto& name = args.at("name").get_ref<const std::string&>();
                   for (const auto& item : list_skills())
                       if (item.connector == id && item.name == name) return item.to_json(true);
                   throw cymcp::Error("NotFound", "Skill not found");
               }, {}});
    tools->add({"mcp_save_skill", "Save a user-requested SKILL.md with YAML name and description for a specific MCP connector. "
                                "Use mcp_list and mcp_tools to inspect the connector first.",
               object_schema({{"id", identifier}, {"markdown", bounded_string(262144)}, {"replace", {{"type", "boolean"}}}},
                             {"id", "markdown"}), output, false,
               [state](const Json& args, const cymcp::CallContext& context) {
                   const auto id = args.at("id").get<std::string>();
                   {
                       std::lock_guard lock(state->mutex);
                       state->refresh(context.cancellation);
                       state->find(id);
                   }
                   return save_skill(id, args.at("markdown"), args.value("replace", false)).to_json();
               }, {}});
    tools->add({"mcp_collect_skills", "Collect SKILL.md resources and skills advertised by the chosen MCP connector into the Hub library.",
               object_schema({{"id", identifier}}, {"id"}), output, false,
               [state](const Json& args, const cymcp::CallContext& context) {
                   const auto id = args.at("id").get<std::string>();
                   Server server;
                   {
                       std::lock_guard lock(state->mutex);
                       state->refresh(context.cancellation);
                       server = state->find(id);
                   }
                   const auto session = state->connection(id, context.cancellation);
                   auto entries = Json::array();
                   for (const auto& item : collect_mcp_skills(server, *session, context.cancellation)) entries.push_back(item.to_json());
                   return Json{{"skills", entries}};
               }, {}});
    tools->add({"mcp_import_skill", "Collect a SKILL.md file from a path relative to the configured workspace.",
               object_schema({{"path", bounded_string(2048)}}, {"path"}), output, false,
               [state](const Json& args, const cymcp::CallContext& context) {
                   context.cancellation.check();
                   const auto path = workspace_target(state->options.workspace, args.at("path"));
                   const auto skill = read_skill_file(path);
                   return save_skill("library", skill.markdown).to_json();
               }, {}});
    tools->add({"sdk_scan", "Find CyMCPSDK references, binary markers and known AI CLIs. Scan an optional file or folder "
                           "relative to the Hub workspace. With no path, list known CLIs in installation directories.",
               object_schema({{"path", bounded_string(2048)}, {"includePath", {{"type", "boolean"}}},
                              {"offset", {{"type", "integer"}, {"minimum", 0}}},
                              {"limit", {{"type", "integer"}, {"minimum", 1}, {"maximum", 100}}}}), output, true,
               [state](const Json& args, const cymcp::CallContext& context) {
                   const auto path = args.value("path", std::string{});
                   const auto target = path.empty() ? hub::fs::path{} : hub::workspace_target(state->options.workspace, path);
                   const auto scan = hub::scan_software(target, args.value("includePath", true), context.cancellation);
                   const auto offset = args.value("offset", std::size_t{0});
                   const auto limit = args.value("limit", std::size_t{50});
                   const auto total = scan.findings.size();
                   const auto size = offset < total ? std::min(limit, total - offset) : 0;
                   auto items = Json::array();
                   for (std::size_t i = 0; i < size; ++i) items.push_back(scan.findings[offset + i].to_json());
                   auto skills = Json::array();
                   for (const auto& skill : scan.skills) skills.push_back(skill.to_json());
                   Json result = {{"findings", items}, {"total", total}, {"filesExamined", scan.examined},
                                  {"unreadable", scan.unreadable}, {"partial", scan.partial}, {"skills", skills}};
                   if (offset < total && size < total - offset) result["nextOffset"] = offset + size;
                   return result;
               }, {}});

    return tools;
}
} // namespace hub
