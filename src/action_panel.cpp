// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "action_panel.hpp"

#include <cyui/Core.h>
#include <algorithm>
#include <array>
#include <cfloat>
#include <utility>

namespace hub {
namespace {
using cyui::tr;
using cyui::trLabel;

std::string text_field(const Json& object, const char* key) {
    const auto found = object.find(key);
    return found != object.end() && found->is_string() ? found->get<std::string>() : std::string{};
}

void show_json(const char* id, const std::string& json, float height) {
    constexpr auto preview_limit = std::size_t{128 * 1024};
    auto preview = json.substr(0, preview_limit);
    if (preview.size() < json.size()) {
        while (!preview.empty() && (static_cast<unsigned char>(json[preview.size()]) & 0xc0) == 0x80) preview.pop_back();
        preview += "\n…";
    }
    const cyui::ScopedFont font(cyui::monoFont());
    ImGui::InputTextMultiline(id, preview.data(), preview.size() + 1, ImVec2(-1, cyui::dp(height)), ImGuiInputTextFlags_ReadOnly);
}
} // namespace

void register_action_translations() {
    cyui::registerTranslations("fr", {
        {"Tools", "Outils"}, {"Resources", "Ressources"}, {"Load tools", "Charger les outils"},
        {"Load resources", "Charger les ressources"}, {"Disconnect", "Déconnecter"}, {"Cancel", "Annuler"},
        {"Run tool", "Exécuter l’action"}, {"Read resource", "Lire la ressource"},
        {"Arguments (JSON)", "Arguments (JSON)"}, {"Input schema", "Schéma des paramètres"}, {"Result", "Résultat"},
        {"Select a resource", "Choisir une ressource"},
        {"Resource URI", "URI de la ressource"}, {"Waiting for the MCP server…", "En attente du serveur MCP…"},
        {"A stdio connection starts a dedicated process.", "Une connexion stdio démarre un processus dédié."},
        {"Tool reported an error", "L’action a signalé une erreur"},
        {"No tools returned", "Le serveur ne renvoie aucun outil"},
        {"No resources returned", "Le serveur ne renvoie aucune ressource"},
        {"Arguments must be a valid JSON object.", "Les arguments doivent former un objet JSON valide."}
    });
}

ActionPanel::~ActionPanel() {
    for (auto& entry : states_) entry.second.cancellation.cancel();
}

void ActionPanel::poll(State& state) {
    if (!state.pending.valid() || state.pending.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    try {
        auto response = state.pending.get();
        switch (state.operation) {
        case Operation::tools: {
            const auto selected = state.tools.empty() ? std::string{} : text_field(state.tools[state.tool_index], "name");
            state.tools = Json::array();
            for (auto& tool : response) if (!text_field(tool, "name").empty()) state.tools.push_back(std::move(tool));
            state.tool_index = 0;
            const auto found = std::find_if(state.tools.begin(), state.tools.end(), [&](const Json& tool) {
                return text_field(tool, "name") == selected;
            });
            if (found == state.tools.end()) state.arguments.clear();
            else state.tool_index = static_cast<int>(found - state.tools.begin());
            if (state.tools.empty()) state.error = tr("No tools returned");
            break;
        }
        case Operation::resources:
            state.resources = std::move(response);
            if (state.resources.empty()) state.error = tr("No resources returned");
            break;
        case Operation::call:
        case Operation::read:
            state.result = response.dump(2, ' ', false, Json::error_handler_t::replace);
            state.resources_result = state.operation == Operation::read;
            if (response.is_object() && response.value("isError", Json()) == true) state.error = tr("Tool reported an error");
            break;
        case Operation::disconnect:
            state.tools = Json::array();
            state.resources = Json::array();
            break;
        }
    } catch (const std::exception& error) {
        state.error = cymcp::redact_error_message(error.what());
    }
}

void ActionPanel::start(State& state, const Server& server, const Options& options, Operation op, std::string name, Json args) {
    if (state.pending.valid()) return;
    if (!state.session) state.session = std::make_shared<McpActions>(server, options, journal_);
    state.cancellation = {};
    state.error.clear();
    state.operation = op;
    if (op == Operation::call || op == Operation::read) {
        state.result.clear();
        state.result_name = name;
    }
    auto session = op == Operation::disconnect ? std::exchange(state.session, nullptr) : state.session;
    state.pending = std::async(std::launch::async,
        [op, session = std::move(session), cancel = state.cancellation, name = std::move(name), args = std::move(args)]() mutable {
            switch (op) {
            case Operation::tools: return session->tools(cancel);
            case Operation::resources: return session->resources(cancel);
            case Operation::call: return session->call(name, args, cancel);
            case Operation::read: return session->read(name, cancel);
            case Operation::disconnect: session.reset(); return Json::object();
            }
            return Json::object();
        });
}

void ActionPanel::draw(const Server& server, const Options& options, bool resources) {
    auto& state = states_[server.id];
    poll(state);
    cyui::space(4);
    {
        const cyui::ScopedDisabled disabled(state.pending.valid() || server.status == Status::disabled);
        if (cyui::button(trLabel(resources ? "Load resources" : "Load tools").c_str()))
            start(state, server, options, resources ? Operation::resources : Operation::tools);
        if (state.session) {
            ImGui::SameLine();
            if (cyui::ghostButton(trLabel("Disconnect").c_str())) start(state, server, options, Operation::disconnect);
        }
    }
    if (state.pending.valid()) {
        ImGui::SameLine();
        if (cyui::ghostButton(trLabel("Cancel").c_str())) state.cancellation.cancel();
        cyui::spinner("##mcp-request", 5);
        ImGui::SameLine();
        cyui::textDim("%s", tr("Waiting for the MCP server…").c_str());
    } else if (!state.session && server.transport == "stdio" && server.status != Status::disabled) {
        cyui::textDim("%s", tr("A stdio connection starts a dedicated process.").c_str());
    }
    if (!state.error.empty()) cyui::callout(cyui::Tone::warning, state.error.c_str());
    if (resources) draw_resources(state, server, options);
    else draw_tools(state, server, options);

    if (state.resources_result == resources && !state.result.empty()) {
        cyui::space(5);
        cyui::sectionTitle(tr("Result").c_str());
        cyui::textDim("%s", state.result_name.c_str());
        show_json("##mcp-result", state.result, 145);
        if (cyui::button(trLabel("Copy JSON").c_str())) {
            ImGui::SetClipboardText(state.result.c_str());
            cyui::notify(tr("Copied").c_str(), cyui::Tone::success);
        }
    }
}

void ActionPanel::draw_tools(State& state, const Server& server, const Options& options) {
    if (state.tools.empty()) return;
    const cyui::ScopedDisabled disabled(state.pending.valid() || server.status == Status::disabled);
    auto reset_arguments = [&] {
        state.arguments.resize(65537);
        std::copy_n("{}", 3, state.arguments.begin());
    };
    if (state.arguments.empty()) reset_arguments();
    ImGui::SetNextItemWidth(-FLT_MIN);
    auto selected_name = text_field(state.tools[state.tool_index], "name");
    if (ImGui::BeginCombo("##mcp-tools", selected_name.c_str())) {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(state.tools.size()));
        while (clipper.Step()) for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const auto name = text_field(state.tools[i], "name");
            const cyui::ScopedID id(i);
            if (ImGui::Selectable(name.c_str(), i == state.tool_index) && i != state.tool_index) {
                state.tool_index = i;
                reset_arguments();
            }
        }
        ImGui::EndCombo();
    }
    const auto& tool = state.tools[state.tool_index];
    const auto name = text_field(tool, "name");
    const auto description = text_field(tool, "description");
    if (!description.empty()) ImGui::TextWrapped("%s", description.c_str());
    if (ImGui::TreeNode(trLabel("Input schema").c_str())) {
        show_json("##mcp-schema", tool.value("inputSchema", Json::object()).dump(2), 120);
        ImGui::TreePop();
    }
    cyui::sectionTitle(tr("Arguments (JSON)").c_str());
    {
        const cyui::ScopedFont font(cyui::monoFont());
        ImGui::InputTextMultiline("##mcp-arguments", state.arguments.data(), state.arguments.size(), ImVec2(-1, cyui::dp(100)));
    }
    if (cyui::primaryButton(trLabel("Run tool").c_str())) {
        auto parameters = Json::parse(state.arguments.data(), nullptr, false);
        if (parameters.is_object()) start(state, server, options, Operation::call, name, std::move(parameters));
        else state.error = tr("Arguments must be a valid JSON object.");
    }
}

void ActionPanel::draw_resources(State& state, const Server& server, const Options& options) {
    const cyui::ScopedDisabled disabled(state.pending.valid() || server.status == Status::disabled);
    if (!state.resources.empty()) {
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##mcp-resources", tr("Select a resource").c_str())) {
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(state.resources.size()));
            while (clipper.Step()) for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const auto uri = text_field(state.resources[i], "uri");
                if (uri.empty()) continue;
                auto name = text_field(state.resources[i], "name");
                const cyui::ScopedID id(i);
                if (ImGui::Selectable(name.empty() ? uri.c_str() : name.c_str(), uri == state.uri)) state.uri = uri;
                cyui::tooltip(uri.c_str());
            }
            ImGui::EndCombo();
        }
    }
    std::array<char, 4097> buffer{};
    std::copy_n(state.uri.begin(), std::min(state.uri.size(), buffer.size() - 1), buffer.begin());
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##mcp-uri", tr("Resource URI").c_str(), buffer.data(), buffer.size())) state.uri = buffer.data();
    const cyui::ScopedDisabled empty(state.uri.empty());
    if (cyui::primaryButton(trLabel("Read resource").c_str())) start(state, server, options, Operation::read, state.uri);
}
} // namespace hub
