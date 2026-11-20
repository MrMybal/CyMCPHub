// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "assistant_panel.hpp"
#include "skills.hpp"
#include "platform.hpp"

#include <cyui/Core.h>
#include <algorithm>

namespace hub {
namespace {
using cyui::tr;
using cyui::trLabel;

Json schema(Json properties, Json required = Json::array()) {
    return {{"type", "object"}, {"properties", std::move(properties)}, {"required", std::move(required)},
            {"additionalProperties", false}};
}

std::shared_ptr<const cymcp::Registry> assistant_tools(const std::optional<Server>& target, const Options& options,
        const std::shared_ptr<Journal>& journal, const std::shared_ptr<AssistantContext>& ui_context = {}) {
    auto commands = std::make_shared<cymcp::Registry>(cymcp::Policy::allow_all());
    const Json output = {{"type", "object"}};
    const Json text = {{"type", "string"}};
    if (ui_context) commands->add({"hub_context", "Read the current tab and selected item in the main CyMCPHub window.",
        schema(Json::object()), output, true, [ui_context](const Json&, const cymcp::CallContext&) {
            std::lock_guard lock(ui_context->mutex);
            return ui_context->value;
        }, {}});
    if (!target) return commands;
    auto session = std::make_shared<McpActions>(*target, options, journal);
    commands->add({"connector_tools", "Find tools and their JSON input schemas on the MCP connector selected by the user.",
        schema({{"query", text}, {"offset", {{"type", "integer"}, {"minimum", 0}}}}), output, true,
        [session](const Json& args, const cymcp::CallContext& context) {
            const auto query = group_key(args.value("query", std::string{}));
            const auto offset = args.value("offset", std::size_t{0});
            auto page = Json::array();
            std::size_t total = 0;
            for (const auto& item : session->tools(context.cancellation)) {
                if (!item.is_object()) continue;
                if (!query.empty() && group_key(item.value("name", std::string{})).find(query) == std::string::npos &&
                    group_key(item.value("description", std::string{})).find(query) == std::string::npos) continue;
                if (total++ >= offset && page.size() < 25) page.push_back(item);
            }
            Json result = {{"tools", page}, {"total", total}};
            if (offset < total && page.size() < total - offset) result["nextOffset"] = offset + page.size();
            return result;
        }, {}});
    commands->add({"connector_call", "Call a tool on the selected MCP connector when requested by the user. Look up the schema first.",
        schema({{"tool", text}, {"arguments", {{"type", "object"}}}}, {"tool"}), output, false,
        [session](const Json& args, const cymcp::CallContext& context) {
            return Json{{"result", session->call(args.at("tool"), args.value("arguments", Json::object()), context.cancellation)}};
        }, {}});
    commands->add({"connector_save_skill", "Save a user-requested SKILL.md for the selected connector. Use YAML name and description, "
                   "and describe workflows using the connector's actual tools and parameters.",
        schema({{"markdown", {{"type", "string"}, {"maxLength", 262144}}}}, {"markdown"}), output, false,
        [id = target->id](const Json& args, const cymcp::CallContext& context) {
            context.cancellation.check();
            return save_skill(id, args.at("markdown")).to_json();
        }, {}});
    return commands;
}
} // namespace

void register_assistant_translations() {
    cyui::registerTranslations("fr", {
        {"Assistant", "Assistant"}, {"Assistant connection", "Connexion de l’assistant"},
        {"Open in Assistant", "Ouvrir dans l’assistant"}, {"Detect providers", "Détecter les fournisseurs"},
        {"Looking for AI providers…", "Recherche de fournisseurs IA…"}, {"Select a provider", "Choisir un fournisseur"},
        {"Connect assistant", "Connecter l’assistant"}, {"No AI provider detected", "Aucun fournisseur IA détecté"},
        {"Selected connector", "Connecteur sélectionné"}, {"Remove target", "Retirer la cible"},
        {"Current tab", "Onglet actif"},
        {"Inspect this connector’s tools. Write a concise SKILL.md with YAML name and description, then save it for this connector.",
         "Inspecte les outils de ce connecteur. Rédige un SKILL.md concis avec un nom et une description YAML, puis enregistre-le pour ce connecteur."},
        {"This provider cannot use the selected connector.", "Ce fournisseur ne peut pas utiliser le connecteur sélectionné."},
        {"Connect to apply the selected provider or connector.", "Connectez-vous pour appliquer le fournisseur ou le connecteur sélectionné."}
    });
}

AssistantPanel::AssistantPanel(std::shared_ptr<Journal> journal)
    : journal_(std::move(journal)), providers_(std::make_shared<cymcp::ProviderRegistry>()), chat_(providers_), panel_(chat_) {
    for (const auto& provider : providers_->describe()) {
        transports_.emplace(provider.at("id").get<std::string>(), provider.at("transport").get<std::string>());
    }
}

AssistantPanel::~AssistantPanel() { discovery_cancel_.cancel(); }

void AssistantPanel::attach(const Server& server) { target_ = server; }

void AssistantPanel::draft_skill(const Server& server) {
    attach(server);
    panel_.set_draft(tr("Inspect this connector’s tools. Write a concise SKILL.md with YAML name and description, then save it for this connector."));
}

void AssistantPanel::set_context(Json context) {
    std::lock_guard lock(context_->mutex);
    context_->value = std::move(context);
}

bool AssistantPanel::can_use_connector(const std::string& provider) const {
    const auto found = transports_.find(provider);
    if (found == transports_.end()) return false;
    const auto& transport = found->second;
    return transport == "app-server-stdio" || transport == "responses-sse" || transport == "messages-sse" ||
           transport == "chat-completions-sse" || transport == "chat-ndjson";
}

void AssistantPanel::discover() {
    discovery_started_ = true;
    discovery_cancel_ = {};
    error_.clear();
    discovering_ = std::async(std::launch::async, [providers = providers_, cancellation = discovery_cancel_] {
        cymcp::CallContext context;
        context.cancellation = cancellation;
        return providers->discover({}, context);
    });
}

void AssistantPanel::poll() {
    chat_.poll();
    for (const auto& item : chat_.state().diagnostics) {
        if (item.id <= last_diagnostic_) continue;
        journal_->add(item.provider_id.empty() ? "Assistant" : item.provider_id, item.operation,
                      item.code.empty() ? item.level : item.code, item.message, item.elapsed_ms);
        last_diagnostic_ = item.id;
    }
    if (!discovering_.valid() || discovering_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    try {
        detected_ = discovering_.get();
        if (std::none_of(detected_.begin(), detected_.end(), [&](const auto& item) {
                return item.available() && can_use_connector(item.provider_id) && item.provider_id == provider_id_;
            })) {
            const auto installed = std::find_if(detected_.begin(), detected_.end(), [](const auto& item) {
                return item.available() && item.provider_id == "codex";
            });
            const auto fallback = installed != detected_.end() ? installed :
                std::find_if(detected_.begin(), detected_.end(), [&](const auto& item) { return item.available() && can_use_connector(item.provider_id); });
            provider_id_ = fallback == detected_.end() ? std::string{} : fallback->provider_id;
        }
    } catch (const std::exception& error) {
        error_ = cymcp::redact_error_message(error.what());
        journal_->add("Assistant", "discover", "Error", error_);
    }
}

void AssistantPanel::connect(const cymcp::DiscoveredConnection& provider, const Options& options) {
    try {
        cymcp::ChatConnection connection;
        connection.provider_id = provider.provider_id;
        connection.provider = provider.config;
        connection.provider.process.cwd = options.workspace;
        if (provider.provider_id == "codex" || provider.provider_id == "codex-exec") {
            const auto home = home_path();
            if (environment_path("CODEX_HOME").empty() && !home.empty()) {
                const auto directory = home / ".codex";
                std::error_code status;
                if (fs::is_directory(directory, status))
                    connection.provider.process.environment.try_emplace("CODEX_HOME", cymcp::path_utf8(directory));
            }
        }
        connection.automatic_model = true;
        const auto target_id = target_ ? target_->id : std::string{};
        connection.provider.tools = assistant_tools(target_, options, journal_, context_);
        connection.system_prompt = "You are the assistant inside CyMCPHub. Call hub_context when helping the user with the current "
                                   "tab or item. If a connector was attached, use connector_tools to inspect it and connector_call to "
                                   "run an action requested by the user. Save a skill only when the user asked you to create one.";
        chat_.open(std::move(connection));
        connected_target_ = target_id;
        error_.clear();
    } catch (const std::exception& error) {
        error_ = cymcp::redact_error_message(error.what());
        journal_->add(provider.provider_id, "connect", "Error", error_);
    }
}

void AssistantPanel::draw(const Options& options) {
    if (!discovery_started_) discover();
    {
        std::lock_guard lock(context_->mutex);
        const auto tab = context_->value.value("tab", std::string{});
        const auto selection = context_->value.value("selection", std::string{});
        const auto detail = context_->value.value("detailTab", std::string{});
        auto label = tr(tab.c_str());
        if (!detail.empty()) label += "  /  " + tr(detail.c_str());
        if (!selection.empty()) label += "  ·  " + selection;
        if (!tab.empty()) cyui::textDim("%s  %s", tr("Current tab").c_str(), label.c_str());
    }
    const auto& state = chat_.state();
    const auto chosen = std::find_if(detected_.begin(), detected_.end(), [&](const auto& item) {
        return item.available() && item.provider_id == provider_id_;
    });
    const auto label = chosen == detected_.end() ? tr("Select a provider") : chosen->name;
    {
        const cyui::ScopedDisabled busy(state.busy);
        ImGui::SetNextItemWidth(cyui::dp(258));
        if (ImGui::BeginCombo("##assistant-provider", label.c_str())) {
            for (const auto& provider : detected_) {
                if (!provider.available() || !can_use_connector(provider.provider_id)) continue;
                const cyui::ScopedID id(provider.provider_id.c_str());
                if (ImGui::Selectable(provider.name.c_str(), provider_id_ == provider.provider_id)) provider_id_ = provider.provider_id;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        const bool supports = !target_ || can_use_connector(provider_id_);
        {
            const cyui::ScopedDisabled unavailable(chosen == detected_.end() || !supports || discovering_.valid());
            if (cyui::primaryButton(trLabel("Connect assistant").c_str())) connect(*chosen, options);
        }
        ImGui::SameLine();
        {
            const cyui::ScopedDisabled discovering(discovering_.valid());
            if (cyui::button(trLabel("Detect providers").c_str())) discover();
        }
        if (target_) {
            const auto& name = target_->server_name.empty() ? target_->name : target_->server_name;
            cyui::textDim("%s  %s · %s", tr("Selected connector").c_str(), name.c_str(), tr(target_->source.c_str()).c_str());
            ImGui::SameLine();
            if (cyui::ghostButton(trLabel("Remove target").c_str())) target_.reset();
        }
        if (!supports && !provider_id_.empty())
            cyui::callout(cyui::Tone::warning, tr("This provider cannot use the selected connector.").c_str());
    }
    if (discovering_.valid()) cyui::textDim("%s", tr("Looking for AI providers…").c_str());
    else if (chosen == detected_.end()) cyui::textDim("%s", tr("No AI provider detected").c_str());
    if (!error_.empty()) cyui::callout(cyui::Tone::error, error_.c_str());
    if (state.connected && (state.provider_id != provider_id_ || connected_target_ != (target_ ? target_->id : std::string{}))) {
        cyui::callout(cyui::Tone::info, tr("Connect to apply the selected provider or connector.").c_str());
        return;
    }
    cyui::divider(3);
    panel_.draw("##assistant-chat");
}
} // namespace hub
