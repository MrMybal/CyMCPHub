// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "ui.hpp"
#include "action_panel.hpp"
#include "assistant_panel.hpp"
#include "resident_service.hpp"
#include "skill_panel.hpp"
#include "sdk_scan.hpp"
#include "platform.hpp"

#include <cyui/Core.h>
#ifdef _WIN32
#include <shobjidl.h>
#include <wrl/client.h>
#else
#include <SDL3/SDL.h>
#endif

#include <algorithm>
#include <array>
#include <cstdio>
#include <ctime>

namespace hub {
namespace {
using cyui::Tone;
using cyui::tr;
using cyui::trLabel;

void register_translations() {
    cyui::registerTranslations("fr", {
        {"Local MCP inventory", "Inventaire MCP local"}, {"Sources", "Sources"},
        {"SDK inventory", "Inventaire SDK"}, {"CLI inventory", "Inventaire CLI"},
        {"Journal", "Journal"}, {"Copy journal", "Copier le journal"}, {"Time", "Heure"},
        {"Operation", "Opération"}, {"Status", "Statut"}, {"Details", "Détails"},
        {"The journal is empty", "Le journal est vide"},
        {"Load a connector’s tools or connect the assistant to see its activity here.",
         "Chargez les outils d’un connecteur ou connectez l’assistant pour voir son activité ici."},
        {"Choose a project folder…", "Choisir un dossier de projet…"},
        {"Add a config file…", "Ajouter un fichier de configuration…"},
        {"Remove the extra config", "Retirer la configuration ajoutée"},
        {"Probe local TCP ports", "Rechercher sur les ports TCP locaux"},
        {"AI setup", "Connexion IA"}, {"Refresh", "Actualiser"}, {"MCP servers", "Serveurs MCP"},
        {"Connectors", "Connecteurs"}, {"Variants", "Variantes"}, {"Search SDKs", "Rechercher un SDK"},
        {"Search CLIs", "Rechercher une CLI"}, {"No file or folder selected", "Aucun fichier ou dossier sélectionné"},
        {"Copy path", "Copier le chemin"}, {"Groups", "Groupes"},
        {"Active", "Actifs"}, {"Analyze folder…", "Analyser un dossier…"},
        {"Analyze file…", "Analyser un fichier…"}, {"Installed CLIs", "CLI installées"},
        {"Include installed CLIs", "Inclure les CLI installées"}, {"Path", "Chemin"}, {"Evidence", "Indices"},
        {"SDK binary", "Binaire avec indices SDK"}, {"SDK library", "Bibliothèque SDK"},
        {"SDK reference", "Référence au SDK"}, {"Known executable name", "Nom d’exécutable connu"},
        {"CLI install directory", "Dossier d’installation CLI"}, {"SDK library filename", "Nom de bibliothèque SDK"},
        {"Software scan in progress…", "Analyse des fichiers en cours…"},
        {"No SDK detected", "Aucun SDK détecté"}, {"No CLI detected", "Aucune CLI détectée"},
        {"Select a result", "Sélectionnez un résultat"},
        {"See its path and detection evidence.", "Consultez son chemin et les indices relevés."},
        {"Choose an application or a folder to look for CyMCPSDK markers and known CLIs.",
         "Choisissez une application ou un dossier pour rechercher des indices CyMCPSDK et des CLI connues."},
        {"Files inspected", "Fichiers inspectés"}, {"Results", "Résultats"},
        {"Scan partially completed", "Analyse partielle"}, {"Unreadable files", "Fichiers illisibles"},
        {"Verified endpoints", "Endpoints confirmés"}, {"Observed stdio processes", "Processus stdio repérés"},
        {"Catalog entries", "Entrées au catalogue"}, {"Servers", "Serveurs"},
        {"Search servers", "Rechercher un serveur"}, {"Active groups only", "Groupes actifs uniquement"},
        {"Scanning…", "Inventaire en cours…"}, {"No matching servers", "Aucun serveur trouvé"},
        {"Try another search or add a config file from Sources.", "Modifiez la recherche ou ajoutez un fichier via Sources."},
        {"Select a server", "Sélectionnez un serveur"},
        {"See its connection details and add an annotation.", "Consultez ses données de connexion et ajoutez une annotation."},
        {"Verified MCP", "MCP confirmé"}, {"Process observed", "Processus repéré"},
        {"Configured", "Configuré"}, {"Authentication required", "Authentification requise"},
        {"Unverified", "Non confirmé"}, {"Disabled", "Désactivé"},
        {"Local TCP", "TCP local"}, {"Custom config", "Configuration ajoutée"},
        {"Connection", "Connexion"}, {"Annotation", "Annotation"}, {"Source", "Source"},
        {"Identity", "Identité"}, {"Protocol", "Protocole"}, {"Connection data", "Données de connexion"},
        {"Copy JSON", "Copier le JSON"}, {"Copied", "Copié"}, {"Save annotation", "Enregistrer l’annotation"},
        {"Annotation saved", "Annotation enregistrée"}, {"Could not save the annotation", "Impossible d’enregistrer l’annotation"},
        {"Annotation changed on disk. Saving will replace it.", "L’annotation a changé sur le disque. Enregistrer remplacera cette version."},
        {"Notes are shared with the AI tools on this computer.", "Les notes sont partagées avec les outils IA sur cet ordinateur."},
        {"Scan failed", "Échec de l’inventaire"}, {"Scan details", "Détails de l’inventaire"},
        {"Some configuration files could not be read.", "Certains fichiers de configuration sont illisibles."},
        {"The process inventory is incomplete.", "L’inventaire des processus est incomplet."},
        {"The TCP inventory is incomplete.", "L’inventaire TCP est incomplet."},
        {"Copy configuration", "Copier la configuration"},
        {"MCP service (HTTP)", "Service MCP (HTTP)"}, {"Start MCP service", "Activer le service MCP"},
        {"Stop MCP service", "Arrêter le service MCP"}, {"Token hidden", "Jeton masqué"},
        {"Copy configuration includes the token.", "La configuration copiée inclut le jeton."},
        {"Add this entry to the MCP configuration of your AI client.", "Ajoutez cette entrée à la configuration MCP de votre client IA."},
        {"The AI can list servers, read connection details, edit annotations and scan for SDKs and CLIs.",
         "L’IA peut lister les serveurs, lire les données de connexion, modifier les annotations et rechercher le SDK et les CLI."},
        {"Last scan", "Dernier inventaire"}, {"Connection metadata omits configured secret values.",
         "Les valeurs des secrets configurés restent dans les fichiers source."}
    });
    cyui::setLocale(french_locale() ? "fr" : "en");
}

const char* status_label(Status state) {
    switch (state) {
    case Status::verified: return "Verified MCP";
    case Status::process_seen: return "Process observed";
    case Status::authentication: return "Authentication required";
    case Status::unverified: return "Unverified";
    case Status::disabled: return "Disabled";
    case Status::configured: return "Configured";
    }
    return "Configured";
}

Tone status_tone(Status state) {
    switch (state) {
    case Status::verified: return Tone::success;
    case Status::process_seen: return Tone::info;
    case Status::authentication:
    case Status::unverified: return Tone::warning;
    default: return Tone::neutral;
    }
}

std::string source_name(const Server& server) {
    if (server.source == "Local TCP" || server.source == "Custom config") return tr(server.source.c_str());
    return server.source;
}

std::string app_mcp_path() {
#ifdef _WIN32
    constexpr auto name = "CyMCPHubMcp.exe";
#else
    constexpr auto name = "CyMCPHubMcp";
#endif
    auto path = executable_path();
    return path.empty() ? name : cymcp::path_utf8(path.replace_filename(name));
}

#ifdef _WIN32
fs::path choose_windows_path(WindowHandle window, bool folder, bool config_only) {
    using Microsoft::WRL::ComPtr;
    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return {};
    FILEOPENDIALOGOPTIONS flags{};
    if (FAILED(dialog->GetOptions(&flags))) return {};
    dialog->SetOptions(flags | FOS_FORCEFILESYSTEM | (folder ? FOS_PICKFOLDERS : FOS_FILEMUSTEXIST));
    const COMDLG_FILTERSPEC filters[] = {{L"MCP configuration", L"*.json;*.jsonc;*.toml"}, {L"All files", L"*.*"}};
    const COMDLG_FILTERSPEC software[] = {{L"Applications and libraries", L"*.exe;*.dll;*.lib;*.a;*.cmd;*.bat;*.ps1"},
                                          {L"All files", L"*.*"}};
    if (!folder) dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), config_only ? filters : software);
    if (FAILED(dialog->Show(window))) return {};
    ComPtr<IShellItem> item;
    if (FAILED(dialog->GetResult(&item))) return {};
    PWSTR path = nullptr;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) return {};
    const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> buffer(path, CoTaskMemFree);
    return fs::path(path);
}
#endif

std::future<fs::path> choose_path(WindowHandle window, bool folder, bool config_only) {
#ifdef _WIN32
    std::promise<fs::path> answer;
    auto result = answer.get_future();
    answer.set_value(choose_windows_path(window, folder, config_only));
    return result;
#else
    auto answer = std::make_unique<std::promise<fs::path>>();
    auto result = answer->get_future();
    const auto completed = +[](void* data, const char* const* files, int) {
        const std::unique_ptr<std::promise<fs::path>> item(static_cast<std::promise<fs::path>*>(data));
        try {
            if (!files) throw std::runtime_error(SDL_GetError());
            item->set_value(*files ? from_utf8(*files) : fs::path{});
        } catch (...) { item->set_exception(std::current_exception()); }
    };
    if (folder) SDL_ShowOpenFolderDialog(completed, answer.release(), window, nullptr, false);
    else {
        static constexpr SDL_DialogFileFilter configs[] = {{"MCP configuration", "json;jsonc;toml"}, {"All files", "*"}};
        SDL_ShowOpenFileDialog(completed, answer.release(), window, config_only ? configs : nullptr,
                               config_only ? static_cast<int>(std::size(configs)) : 0, nullptr, false);
    }
    return result;
#endif
}

void wrapped(const std::string& text) { ImGui::TextWrapped("%s", text.c_str()); }

struct Draft { std::string original, text; };
struct Group {
    std::string key, name;
    std::vector<Server*> variants;
};

struct SoftwareGroup {
    std::string key, name;
    std::vector<const SoftwareFinding*> variants;
};

struct Metric {
    const char* id;
    const char* label;
    std::size_t count;
    Tone tone;
};

enum View { mcp, sdk, cli, skills_view, journal };
constexpr const char* view_labels[] = {"MCP servers", "SDK", "CLI", "Skills", "Journal"};

class VariantButtons {
    const float width_ = ImGui::GetContentRegionAvail().x;
    const float end_ = ImGui::GetCursorScreenPos().x + width_;
    float previous_end_ = end_;

public:
    bool draw(const std::string& label, const std::string& id, bool active) {
        const auto width = std::min(width_, cyui::buttonWidth(label.c_str()));
        if (previous_end_ + ImGui::GetStyle().ItemSpacing.x + width <= end_) ImGui::SameLine();
        const auto control = label + "###" + id;
        const bool clicked = cyui::button(control.c_str(), active ? cyui::Variant::primary : cyui::Variant::secondary, ImVec2(width, 0));
        previous_end_ = ImGui::GetItemRectMax().x;
        return clicked;
    }
};

const char* software_type(const std::string& kind) {
    if (kind == "sdk_binary") return "SDK binary";
    if (kind == "sdk_library") return "SDK library";
    if (kind == "sdk_reference") return "SDK reference";
    return "CLI";
}

class Window {
    enum class Pick { workspace, configuration, software_folder, software_file };
    WindowHandle window_;
    Options options_;
    Snapshot snapshot_;
    std::future<Snapshot> pending_;
    std::future<fs::path> pending_path_;
    Pick pick_ = Pick::workspace;
    cymcp::Cancellation cancellation_;
    SoftwareScan software_;
    std::future<SoftwareScan> pending_software_;
    cymcp::Cancellation software_cancellation_;
    fs::path software_target_;
    std::string error_, last_scan_, selected_group_, software_error_;
    std::array<char, 256> search_{};
    std::array<std::array<char, 256>, 2> software_search_{};
    std::array<std::string, 2> selected_software_group_;
    std::map<std::string, Draft> drafts_;
    std::map<std::string, std::string> selected_connectors_;
    std::map<std::string, std::string> selected_software_variants_;
    std::shared_ptr<Journal> journal_ = std::make_shared<Journal>();
    ResidentService resident_{journal_};
    ActionPanel actions_{journal_};
    AssistantPanel assistant_{journal_};
    SkillPanel skills_{journal_};
    Json current_context_ = Json::object();
    bool active_only_ = false, include_clis_ = true, software_scanned_ = false;
    int view_ = mcp;
    int setup_transport_ = 0;
    bool assistant_open_ = false, assistant_focus_ = false;

public:
    Window(WindowHandle window, Options options) : window_(window), options_(std::move(options)) {
        resident_.restore(options_);
        refresh();
    }
    ~Window() { cancellation_.cancel(); software_cancellation_.cancel(); }

    void draw() {
        poll();
        poll_software();
        poll_path();
        assistant_.poll();
        if (tray_assistant_requested) {
            tray_assistant_requested = false;
            open_assistant();
        }
        current_context_ = Json::object();
        const auto* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        const cyui::ScopedVar padding(ImGuiStyleVar_WindowPadding, cyui::dp(19, 15));
        ImGui::Begin("##hub", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                                       ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus);
        draw_header();
        current_context_["tab"] = view_labels[view_];
        if (view_ == skills_view) {
            cyui::heading(tr("Skill library").c_str());
            current_context_["selection"] = skills_.draw(nullptr, options_, software_.skills, {});
        } else if (view_ == journal) {
            draw_journal();
        } else if (view_ == sdk || view_ == cli) {
            draw_software();
        } else {
            draw_metrics();
            std::vector<Group> groups;
            draw_split([&] { draw_list(groups); }, [&] { draw_details(groups); });
            draw_footer();
        }
        ImGui::End();
        assistant_.set_context(std::move(current_context_));
    }

    bool& assistant_open() { return assistant_open_; }
    bool& assistant_focus() { return assistant_focus_; }

    void draw_assistant_window() {
        if (!assistant_open_) return;
        const auto* main = ImGui::GetMainViewport();
        const auto title = tr("Assistant") + " · CyMCPHub###hub-assistant";
#ifdef _WIN32
        ImGui::SetNextWindowSize(cyui::dp(700, 620), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(main->WorkPos.x + cyui::dp(85), main->WorkPos.y + cyui::dp(62)), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(cyui::dp(465, 380), ImVec2(10000, 10000));
        ImGuiWindowClass external;
        external.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;
        external.ViewportFlagsOverrideClear = ImGuiViewportFlags_NoDecoration | ImGuiViewportFlags_NoTaskBarIcon;
        ImGui::SetNextWindowClass(&external);
        if (assistant_focus_) ImGui::SetNextWindowFocus();
        assistant_focus_ = false;
        const auto flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
#else
        ImGui::SetNextWindowPos(main->Pos);
        ImGui::SetNextWindowSize(main->Size);
        const auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
#endif
        if (ImGui::Begin(title.c_str(), &assistant_open_, flags)) assistant_.draw(options_);
        ImGui::End();
    }

private:
    void open_assistant() {
        assistant_open_ = true;
        assistant_focus_ = true;
    }

    void pick_path(Pick action) {
        if (pending_path_.valid()) return;
        pick_ = action;
        pending_path_ = choose_path(window_, action == Pick::workspace || action == Pick::software_folder,
                                    action == Pick::configuration);
    }

    void poll_path() {
        if (!pending_path_.valid() || pending_path_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        try {
            auto path = pending_path_.get();
            if (path.empty()) return;
            switch (pick_) {
            case Pick::workspace: options_.workspace = path.lexically_normal(); refresh(); break;
            case Pick::configuration: options_.extra_config = path.lexically_normal(); refresh(); break;
            case Pick::software_folder: case Pick::software_file:
                software_target_ = std::move(path);
                refresh_software();
                break;
            }
        } catch (const std::exception& error) {
            cyui::notify(cymcp::redact_error_message(error.what()).c_str(), Tone::error);
        }
    }

    template <class List, class Details>
    void draw_split(List list, Details details) {
        const auto available = ImGui::GetContentRegionAvail();
        const float left = std::clamp(available.x * 0.32f, cyui::dp(260), cyui::dp(365));
        const float height = std::max(available.y - ImGui::GetFrameHeightWithSpacing() - cyui::dp(6), cyui::dp(90));
        const cyui::ScopedVar rounding(ImGuiStyleVar_ChildRounding, cyui::dp(8));
        if (ImGui::BeginChild("##catalog", ImVec2(left, height), ImGuiChildFlags_Borders)) list();
        ImGui::EndChild();
        ImGui::SameLine(0, cyui::dp(13));
        if (ImGui::BeginChild("##details", ImVec2(0, height), ImGuiChildFlags_Borders)) details();
        ImGui::EndChild();
    }

    void refresh() {
        if (pending_.valid()) return;
        resident_.sync_options(options_);
        error_.clear();
        cancellation_ = {};
        pending_ = std::async(std::launch::async, [options = options_, cancel = cancellation_] {
            return scan(options, cancel);
        });
    }

    void poll() {
        if (!pending_.valid() || pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        try {
            snapshot_ = pending_.get();
            last_scan_ = local_time(std::time(nullptr));
        } catch (const std::exception& error) {
            error_ = cymcp::redact_error_message(error.what());
        }
    }

    void refresh_software() {
        if (pending_software_.valid()) return;
        software_scanned_ = true;
        software_error_.clear();
        software_cancellation_ = {};
        pending_software_ = std::async(std::launch::async,
            [target = software_target_, include = include_clis_, cancel = software_cancellation_] {
                return scan_software(target, include, cancel);
            });
    }

    void poll_software() {
        if (!pending_software_.valid() || pending_software_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        try { software_ = pending_software_.get(); }
        catch (const std::exception& error) { software_error_ = cymcp::redact_error_message(error.what()); }
    }

    void draw_header() {
        const auto sources = trLabel("Sources"), setup = trLabel("AI setup"), reload = trLabel("Refresh");
        const auto assistant_button = trLabel("Assistant");
        const auto spacing = ImGui::GetStyle().ItemSpacing.x;
        const bool refresh_visible = view_ == mcp || view_ == sdk || view_ == cli;
        const auto width = cyui::buttonWidth(setup.c_str()) + cyui::buttonWidth(assistant_button.c_str()) + spacing +
                           (refresh_visible ? cyui::buttonWidth(reload.c_str()) + spacing : 0) +
                           (view_ == mcp ? cyui::buttonWidth(sources.c_str()) + spacing : 0);
        cyui::heading("CyMCPHub", 1);
        ImGui::SameLine();
        cyui::alignRight(width);
        if (view_ == mcp) {
            draw_sources(sources);
            ImGui::SameLine();
        }
        if (cyui::button(assistant_button.c_str())) open_assistant();
        ImGui::SameLine();
        if (cyui::button(setup.c_str())) ImGui::OpenPopup("AI setup###setup");
        if (refresh_visible) {
            ImGui::SameLine();
            const cyui::ScopedDisabled scanning(pending_path_.valid() ||
                (view_ == mcp ? pending_.valid() : pending_software_.valid()));
            if (cyui::primaryButton(reload.c_str())) {
                if (view_ == mcp) refresh();
                else refresh_software();
            }
        }
        const char* subtitles[] = {"Local MCP inventory", "SDK inventory", "CLI inventory", "Skill library", "Journal"};
        cyui::textDim("%s", tr(subtitles[view_]).c_str());
        cyui::space(5);
        if (cyui::segmented("##view", view_labels, static_cast<int>(std::size(view_labels)), &view_) &&
            (view_ == sdk || view_ == cli) && !software_scanned_)
            refresh_software();
        draw_setup();
        cyui::divider(6);
    }

    void draw_sources(const std::string& label) {
        if (cyui::button(label.c_str())) ImGui::OpenPopup("##sources");
        if (!ImGui::BeginPopup("##sources")) return;
        {
            const cyui::ScopedDisabled scanning(pending_.valid() || pending_path_.valid());
            if (ImGui::MenuItem(trLabel("Choose a project folder…").c_str())) pick_path(Pick::workspace);
            if (ImGui::MenuItem(trLabel("Add a config file…").c_str())) pick_path(Pick::configuration);
            if (!options_.extra_config.empty() && ImGui::MenuItem(trLabel("Remove the extra config").c_str())) {
                options_.extra_config.clear();
                refresh();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(trLabel("Probe local TCP ports").c_str(), nullptr, &options_.scan_ports)) refresh();
        }
        ImGui::EndPopup();
    }

    void draw_setup() {
        std::string title = tr("AI setup") + "###setup";
        ImGui::SetNextWindowSize(cyui::dp(610, 410), ImGuiCond_Appearing);
        bool open = true;
        if (!ImGui::BeginPopupModal(title.c_str(), &open)) return;
        wrapped(tr("Add this entry to the MCP configuration of your AI client."));
        const char* modes[] = {"stdio", "MCP service (HTTP)"};
        cyui::segmented("##mcp-transport", modes, 2, &setup_transport_);
        const bool http = setup_transport_ == 1;
        if (http) {
            if (cyui::button(trLabel(resident_.running() ? "Stop MCP service" : "Start MCP service").c_str()))
                resident_.toggle(options_);
            if (!resident_.error().empty()) cyui::callout(Tone::error, resident_.error().c_str());
            if (!resident_.running()) {
                ImGui::EndPopup();
                return;
            }
        }
        auto args = Json::array({"--workspace", cymcp::path_utf8(options_.workspace)});
        if (!options_.extra_config.empty()) {
            args.push_back("--config");
            args.push_back(cymcp::path_utf8(options_.extra_config));
        }
        if (!options_.scan_ports) args.push_back("--no-port-scan");
        auto config = http ? resident_.configuration() :
            Json({{"mcpServers", {{"cymcphub", {{"command", app_mcp_path()}, {"args", args}}}}}});
        auto displayed = config;
        if (http) displayed["mcpServers"]["cymcphub"]["headers"]["Authorization"] = "Bearer <" + tr("Token hidden") + ">";
        auto json = displayed.dump(2);
        ImGui::InputTextMultiline("##setup-json", json.data(), json.size() + 1,
                                  ImVec2(-1, cyui::dp(130)), ImGuiInputTextFlags_ReadOnly);
        wrapped(tr(http ? "Copy configuration includes the token." :
                    "The AI can list servers, read connection details, edit annotations and scan for SDKs and CLIs."));
        if (cyui::primaryButton(trLabel("Copy configuration").c_str())) {
            ImGui::SetClipboardText(config.dump(2).c_str());
            cyui::notify(tr("Copied").c_str(), Tone::success);
        }
        ImGui::EndPopup();
    }

    void draw_metrics() {
        const auto verified = std::count_if(snapshot_.servers.begin(), snapshot_.servers.end(),
                                            [](const Server& server) { return server.status == Status::verified; });
        const auto processes = std::count_if(snapshot_.servers.begin(), snapshot_.servers.end(),
                                             [](const Server& server) { return server.status == Status::process_seen; });
        draw_metrics({{{"##verified", "Verified endpoints", static_cast<std::size_t>(verified), Tone::success},
                        {"##processes", "Observed stdio processes", static_cast<std::size_t>(processes), Tone::info},
                        {"##entries", "Catalog entries", snapshot_.servers.size(), Tone::accent}}});
    }

    static void draw_metrics(const std::array<Metric, 3>& metrics) {
        if (ImGui::BeginTable("##metrics", static_cast<int>(metrics.size()), ImGuiTableFlags_SizingStretchSame)) {
            for (const auto& metric : metrics) {
                ImGui::TableNextColumn();
                if (cyui::beginCard(metric.id)) {
                    cyui::statusDot(cyui::toneColour(metric.tone));
                    ImGui::SameLine();
                    cyui::textDim("%s", tr(metric.label).c_str());
                    cyui::heading(std::to_string(metric.count).c_str(), 1);
                }
                cyui::endCard();
            }
            ImGui::EndTable();
        }
        cyui::space(5);
    }

    std::vector<Group> visible_groups() {
        std::vector<Group> groups;
        std::map<std::string, std::size_t> indices;
        for (auto& server : snapshot_.servers) {
            const auto key = group_key(server.name);
            const auto [item, inserted] = indices.try_emplace(key, groups.size());
            if (inserted) groups.push_back({key, server.name, {}});
            auto& group = groups[item->second];
            if (!server.server_name.empty() && group_key(server.server_name) == key) group.name = server.server_name;
            group.variants.push_back(&server);
        }
        std::erase_if(groups, [&](const Group& group) {
            return std::none_of(group.variants.begin(), group.variants.end(), [&](const Server* server) {
                return (!active_only_ || is_active(server->status)) && matches(*server, search_.data());
            });
        });
        return groups;
    }

    void draw_list(std::vector<Group>& groups) {
        cyui::heading(tr("Servers").c_str());
        cyui::searchField("##search", search_.data(), static_cast<int>(search_.size()), tr("Search servers").c_str());
        ImGui::Checkbox(trLabel("Active groups only").c_str(), &active_only_);
        cyui::divider(3);

        groups = visible_groups();
        if (groups.empty()) {
            selected_group_.clear();
            if (!pending_.valid()) cyui::emptyState(tr("No matching servers").c_str(),
                                                     tr("Try another search or add a config file from Sources.").c_str());
            return;
        }
        if (std::none_of(groups.begin(), groups.end(), [&](const Group& item) { return item.key == selected_group_; }))
            select_group(groups.front());

        if (ImGui::BeginChild("##rows")) {
            for (const auto& group : groups) draw_row(group);
        }
        ImGui::EndChild();
    }

    void select_group(const Group& group) {
        selected_group_ = group.key;
    }

    void draw_row(const Group& group) {
        const auto& server = *group.variants.front();
        const auto active_count = std::count_if(group.variants.begin(), group.variants.end(),
                                                [](const Server* item) { return is_active(item->status); });
        const auto subtitle = group.variants.size() > 1 ? tr("Connectors") + "  ·  " + tr("Active") + " " + std::to_string(active_count) :
                                  source_name(server) + "  ·  " + server.transport + "  ·  " + tr(status_label(server.status));
        if (draw_catalog_row(group.key, group.name, subtitle, group.variants.size(), status_tone(server.status),
                             selected_group_ == group.key)) select_group(group);
    }

    static bool draw_catalog_row(const std::string& key, const std::string& name, const std::string& subtitle,
                                 std::size_t variants, Tone tone, bool active) {
        const auto& colors = cyui::palette();
        const cyui::ScopedID id(key.c_str());
        const bool clicked = ImGui::Selectable("##entry", active, ImGuiSelectableFlags_None, ImVec2(0, cyui::dp(49)));
        const auto min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
        auto* draw = ImGui::GetWindowDrawList();
        if (active)
            draw->AddRectFilled(ImVec2(min.x, min.y), ImVec2(min.x + cyui::dp(3), max.y), colors.accent, cyui::dp(1));
        draw->AddCircleFilled(ImVec2(min.x + cyui::dp(15), min.y + cyui::dp(17)), cyui::dp(3.5f),
                              cyui::toneColour(tone));
        const float text_x = min.x + cyui::dp(28), width = max.x - text_x - cyui::dp(8);
        const auto count = std::to_string(variants);
        const auto count_width = ImGui::CalcTextSize(count.c_str()).x + cyui::dp(14);
        const bool grouped = variants > 1;
        if (grouped) {
            const auto right = max.x - cyui::dp(9);
            const ImVec2 pill_min(right - count_width, min.y + cyui::dp(5));
            const ImVec2 pill_max(right, pill_min.y + ImGui::GetTextLineHeight() + cyui::dp(4));
            draw->AddRectFilled(pill_min, pill_max, colors.bg3, cyui::dp(6));
            cyui::centredText(draw, pill_min, pill_max, colors.text, count.c_str());
        }
        cyui::clippedText(draw, ImVec2(text_x, min.y + cyui::dp(7)), width - (grouped ? count_width + cyui::dp(5) : 0),
                          colors.text, name.c_str());
        cyui::clippedText(draw, ImVec2(text_x, min.y + cyui::dp(28)), width, colors.textDim, subtitle.c_str());
        cyui::tooltip(subtitle.c_str());
        draw->AddLine(ImVec2(min.x + cyui::dp(9), max.y), ImVec2(max.x, max.y), colors.line);
        return clicked;
    }

    void draw_details(const std::vector<Group>& groups) {
        const auto group = std::find_if(groups.begin(), groups.end(), [&](const Group& item) { return item.key == selected_group_; });
        if (group == groups.end()) {
            cyui::emptyState(tr(pending_.valid() ? "Scanning…" : "Select a server").c_str(),
                             pending_.valid() ? "" : tr("See its connection details and add an annotation.").c_str());
            return;
        }
        cyui::heading(group->name.c_str(), 1);
        current_context_["selection"] = group->name;
        const cyui::ScopedID group_id(group->key.c_str());
        cyui::space(3);
        cyui::sectionTitle(tr("Connectors").c_str());
        ImGui::SameLine();
        cyui::badge(std::to_string(group->variants.size()).c_str());
        draw_variant(draw_connectors(*group));
    }

    Server& draw_connectors(const Group& group) {
        auto& selected_id = selected_connectors_[group.key];
        const auto current = std::find_if(group.variants.begin(), group.variants.end(), [&](const Server* server) {
            return server->id == selected_id;
        });
        auto* selected = current == group.variants.end() ? group.variants.front() : *current;
        VariantButtons buttons;
        std::map<std::string, int> seen_labels;
        for (auto* server : group.variants) {
            auto label = source_name(*server) + " · " + server->transport;
            const auto duplicate = ++seen_labels[label];
            if (duplicate > 1) label += " #" + std::to_string(duplicate);
            if (buttons.draw(label, server->id, server == selected)) selected = server;
            if (!server->source_file.empty()) label += '\n' + cymcp::path_utf8(server->source_file);
            cyui::tooltip(label.c_str());
        }
        selected_id = selected->id;
        return *selected;
    }

    void draw_variant(Server& server) {
        const cyui::ScopedID id(server.id.c_str());
        current_context_["connectorId"] = server.id;
        current_context_["connectorSource"] = server.source;
        current_context_["transport"] = server.transport;
        current_context_["status"] = status_key(server.status);
        cyui::space(5);
        cyui::badge(tr(status_label(server.status)).c_str(), status_tone(server.status));
        ImGui::SameLine();
        cyui::badge(server.transport.c_str());
        if (server.pid) {
            ImGui::SameLine();
            cyui::textFaint("PID %lu", server.pid);
        }
        {
            const auto action = trLabel("Open in Assistant");
            ImGui::SameLine();
            cyui::alignRight(cyui::buttonWidth(action.c_str()));
            const cyui::ScopedDisabled disabled(server.status == Status::disabled);
            if (cyui::button(action.c_str())) {
                assistant_.attach(server);
                open_assistant();
            }
        }
        cyui::divider(5);
        const auto& palette = cyui::palette();
        const auto selected_tab = cyui::mix(palette.bg2, palette.accent, 0.28f);
        cyui::ScopedColor tabs(ImGuiCol_TabSelected, selected_tab);
        tabs.push(ImGuiCol_TabDimmedSelected, selected_tab)
            .push(ImGuiCol_TabSelectedOverline, palette.accent)
            .push(ImGuiCol_TabDimmedSelectedOverline, palette.accent);
        if (!ImGui::BeginTabBar("##detail-tabs", ImGuiTabBarFlags_DrawSelectedOverline)) return;
        if (ImGui::BeginTabItem(trLabel("Connection").c_str())) {
            current_context_["detailTab"] = "Connection";
            draw_connection(server);
            ImGui::EndTabItem();
        }
        const auto annotation = tr("Annotation") + (server.note.empty() ? "" : "  •") + "###annotation";
        if (ImGui::BeginTabItem(annotation.c_str())) {
            current_context_["detailTab"] = "Annotation";
            draw_annotation(server);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(trLabel("Tools").c_str())) {
            current_context_["detailTab"] = "Tools";
            actions_.draw(server, options_, false);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(trLabel("Resources").c_str())) {
            current_context_["detailTab"] = "Resources";
            actions_.draw(server, options_, true);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(trLabel("Skills").c_str())) {
            current_context_["detailTab"] = "Skills";
            current_context_["skill"] = skills_.draw(&server, options_, {}, [&](const Server& connector) {
                assistant_.draft_skill(connector);
                open_assistant();
            });
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    void draw_software() {
        const bool scanning = pending_software_.valid();
        const bool cli_view = view_ == cli;
        const cyui::ScopedID view_id(view_);
        {
            const cyui::ScopedDisabled disabled(scanning || pending_path_.valid());
            if (cyui::button(trLabel("Analyze folder…").c_str())) pick_path(Pick::software_folder);
            ImGui::SameLine();
            if (cyui::button(trLabel("Analyze file…").c_str())) pick_path(Pick::software_file);
            if (!software_target_.empty()) {
                ImGui::SameLine();
                if (cyui::ghostButton(trLabel("Installed CLIs").c_str())) {
                    software_target_.clear();
                    include_clis_ = true;
                    refresh_software();
                }
                ImGui::SameLine();
                if (ImGui::Checkbox(trLabel("Include installed CLIs").c_str(), &include_clis_)) refresh_software();
            }
        }
        const auto target = software_target_.empty() ? tr(cli_view ? "Installed CLIs" : "No file or folder selected") :
                                                       cymcp::path_utf8(software_target_);
        cyui::divider(3);

        const auto groups = software_groups(cli_view);
        const auto findings = std::count_if(software_.findings.begin(), software_.findings.end(), [&](const SoftwareFinding& item) {
            return (item.kind == "cli") == cli_view;
        });
        draw_metrics({{{"##software-groups", "Groups", groups.size(), Tone::accent},
                        {"##software-results", "Results", static_cast<std::size_t>(findings), Tone::success},
                        {"##software-inspected", "Files inspected", software_.examined, Tone::info}}});
        const auto index = static_cast<std::size_t>(view_ - 1);
        draw_split([&] { draw_software_list(groups, index, cli_view); }, [&] { draw_software_details(groups, index); });

        if (scanning) {
            cyui::spinner("##software-progress", 5);
            ImGui::SameLine();
            cyui::textDim("%s", tr("Software scan in progress…").c_str());
        } else if (!software_error_.empty()) {
            cyui::textColoured(cyui::toneColour(Tone::error), "%s", tr("Scan failed").c_str());
            cyui::tooltip(software_error_.c_str());
        } else {
            const auto leaf = software_target_.filename();
            cyui::textDim("%s", leaf.empty() ? target.c_str() : cymcp::path_utf8(leaf).c_str());
            cyui::tooltip(target.c_str());
        }
        if (software_.partial) {
            ImGui::SameLine();
            cyui::badge(tr("Scan partially completed").c_str(), Tone::warning);
        }
        if (software_.unreadable) {
            ImGui::SameLine();
            cyui::textDim("%s  %zu", tr("Unreadable files").c_str(), software_.unreadable);
        }
        if (!software_.skills.empty()) {
            ImGui::SameLine();
            const auto label = tr("Skills") + "  (" + std::to_string(software_.skills.size()) + ")###software-skills";
            if (cyui::ghostButton(label.c_str())) view_ = skills_view;
        }
    }

    std::vector<SoftwareGroup> software_groups(bool cli) const {
        std::vector<SoftwareGroup> groups;
        std::map<std::string, std::size_t> indices;
        for (const auto& item : software_.findings) {
            if ((item.kind == "cli") != cli) continue;
            const auto name = cli ? item.name : cymcp::path_utf8(item.path.filename());
            const auto key = group_key(name);
            const auto [index, inserted] = indices.try_emplace(key, groups.size());
            if (inserted) groups.push_back({key, name, {}});
            groups[index->second].variants.push_back(&item);
        }
        return groups;
    }

    void draw_software_list(const std::vector<SoftwareGroup>& groups, std::size_t index, bool cli) {
        cyui::heading(cli ? "CLI" : "SDK");
        auto& search = software_search_[index];
        cyui::searchField("##software-search", search.data(), static_cast<int>(search.size()), tr(cli ? "Search CLIs" : "Search SDKs").c_str());
        cyui::divider(3);
        const auto query = group_key(search.data());
        auto& selected = selected_software_group_[index];
        const auto match = [&](const SoftwareGroup& group) {
            if (query.empty() || group.key.find(query) != std::string::npos) return true;
            return std::any_of(group.variants.begin(), group.variants.end(), [&](const SoftwareFinding* item) {
                return group_key(item->name).find(query) != std::string::npos ||
                       group_key(cymcp::path_utf8(item->path)).find(query) != std::string::npos;
            });
        };
        if (std::none_of(groups.begin(), groups.end(), [&](const SoftwareGroup& group) { return group.key == selected && match(group); })) {
            const auto first = std::find_if(groups.begin(), groups.end(), match);
            selected = first == groups.end() ? std::string{} : first->key;
        }
        if (selected.empty()) {
            if (!pending_software_.valid()) cyui::emptyState(tr(cli ? "No CLI detected" : "No SDK detected").c_str(),
                tr("Choose an application or a folder to look for CyMCPSDK markers and known CLIs.").c_str());
            return;
        }
        if (ImGui::BeginChild("##rows")) for (const auto& group : groups) {
            if (!match(group)) continue;
            const auto* first = group.variants.front();
            const auto label = group.variants.size() == 1 ? tr(software_type(first->kind)) : tr("Variants");
            if (draw_catalog_row(group.key, group.name, label, group.variants.size(), Tone::info, selected == group.key)) selected = group.key;
        }
        ImGui::EndChild();
    }

    void draw_software_details(const std::vector<SoftwareGroup>& groups, std::size_t index) {
        const auto& active = selected_software_group_[index];
        const auto group = std::find_if(groups.begin(), groups.end(), [&](const SoftwareGroup& item) { return item.key == active; });
        if (group == groups.end()) {
            if (pending_software_.valid()) cyui::emptyState(tr("Software scan in progress…").c_str(), "");
            else cyui::emptyState(tr("Select a result").c_str(), tr("See its path and detection evidence.").c_str());
            return;
        }
        const cyui::ScopedID id(group->key.c_str());
        cyui::heading(group->name.c_str(), 1);
        current_context_["selection"] = group->name;
        cyui::space(3);
        cyui::sectionTitle(tr("Variants").c_str());
        ImGui::SameLine();
        cyui::badge(std::to_string(group->variants.size()).c_str());
        auto& selected_id = selected_software_variants_[std::to_string(index) + group->key];
        const auto key = [](const SoftwareFinding& item) { return cymcp::path_utf8(item.path) + '#' + item.kind; };
        const auto selected_it = std::find_if(group->variants.begin(), group->variants.end(), [&](const auto* item) {
            return key(*item) == selected_id;
        });
        const auto* selected = selected_it == group->variants.end() ? group->variants.front() : *selected_it;
        VariantButtons buttons;
        std::map<std::string, int> names;
        for (const auto* item : group->variants) {
            auto label = item->kind == "cli" ? cymcp::path_utf8(item->path.filename()) : tr(software_type(item->kind));
            const auto duplicate = ++names[label];
            if (duplicate > 1) label += " #" + std::to_string(duplicate);
            if (buttons.draw(label, key(*item), item == selected)) selected = item;
            cyui::tooltip(cymcp::path_utf8(item->path).c_str());
        }
        selected_id = key(*selected);
        cyui::space(5);
        cyui::badge(tr(software_type(selected->kind)).c_str(), Tone::info);
        cyui::divider(5);
        const auto path = cymcp::path_utf8(selected->path);
        if (cyui::beginProperties("##software-properties", 106)) {
            cyui::property(tr("Path").c_str());
            wrapped(path);
            cyui::property(tr("Evidence").c_str());
            for (const auto& item : selected->evidence) wrapped(tr(item.c_str()));
            cyui::endProperties();
        }
        if (cyui::button(trLabel("Copy path").c_str())) {
            ImGui::SetClipboardText(path.c_str());
            cyui::notify(tr("Copied").c_str(), Tone::success);
        }
    }

    void draw_connection(const Server& server) {
        cyui::space(5);
        if (cyui::beginProperties("##metadata", 106)) {
            cyui::property(tr("Source").c_str());
            wrapped(source_name(server));
            if (!server.source_file.empty()) wrapped(cymcp::path_utf8(server.source_file));
            if (!server.server_name.empty()) {
                cyui::property(tr("Identity").c_str());
                wrapped(server.server_name + "  /  " + server.version);
            }
            if (!server.protocol.empty()) {
                cyui::property(tr("Protocol").c_str());
                wrapped(server.protocol);
            }
            cyui::endProperties();
        }
        cyui::space(7);
        cyui::sectionTitle(tr("Connection data").c_str());
        std::string json = server.connection.dump(2);
        {
            const cyui::ScopedFont mono(cyui::monoFont());
            ImGui::InputTextMultiline("##connection-data", json.data(), json.size() + 1,
                                      ImVec2(-1, cyui::dp(144)), ImGuiInputTextFlags_ReadOnly);
        }
        if (cyui::button(trLabel("Copy JSON").c_str())) {
            ImGui::SetClipboardText(json.c_str());
            cyui::notify(tr("Copied").c_str(), Tone::success);
        }
        if (!server.source_file.empty()) wrapped(tr("Connection metadata omits configured secret values."));
    }

    void draw_annotation(Server& server) {
        cyui::space(5);
        auto& draft = drafts_.try_emplace(server.id, Draft{server.note, server.note}).first->second;
        if (draft.text == draft.original && draft.original != server.note) draft = {server.note, server.note};
        wrapped(tr("Notes are shared with the AI tools on this computer."));
        const cyui::ScopedID id(server.id.c_str());
        std::array<char, 8193> text{};
        std::copy_n(draft.text.begin(), std::min(draft.text.size(), text.size() - 1), text.begin());
        if (ImGui::InputTextMultiline("##annotation-text", text.data(), text.size(), ImVec2(-1, cyui::dp(160))))
            draft.text = text.data();
        if (draft.original != server.note) cyui::callout(Tone::warning, tr("Annotation changed on disk. Saving will replace it.").c_str());
        const cyui::ScopedDisabled disabled(draft.text == server.note);
        if (cyui::primaryButton(trLabel("Save annotation").c_str())) {
            try {
                set_note(server.id, draft.text);
                server.note = draft.original = draft.text;
                cyui::notify(tr("Annotation saved").c_str(), Tone::success);
            } catch (const std::exception&) {
                cyui::notify(tr("Could not save the annotation").c_str(), Tone::error);
            }
        }
    }

    void draw_journal() const {
        const auto entries = journal_->entries();
        if (entries.empty()) {
            cyui::emptyState(tr("The journal is empty").c_str(),
                             tr("Load a connector’s tools or connect the assistant to see its activity here.").c_str());
            return;
        }
        if (cyui::button(trLabel("Copy journal").c_str())) {
            ImGui::SetClipboardText(journal_->json_lines().c_str());
            cyui::notify(tr("Copied").c_str(), Tone::success);
        }
        if (!ImGui::BeginTable("##journal", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                               ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) return;
        ImGui::TableSetupColumn(tr("Time").c_str(), ImGuiTableColumnFlags_WidthFixed, cyui::dp(82));
        ImGui::TableSetupColumn(tr("Source").c_str(), ImGuiTableColumnFlags_WidthStretch, 1.15f);
        ImGui::TableSetupColumn(tr("Operation").c_str(), ImGuiTableColumnFlags_WidthStretch, 1.15f);
        ImGui::TableSetupColumn(tr("Status").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.75f);
        ImGui::TableSetupColumn("ms", ImGuiTableColumnFlags_WidthFixed, cyui::dp(48));
        ImGui::TableSetupColumn(tr("Details").c_str(), ImGuiTableColumnFlags_WidthStretch, 1.8f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        for (auto item = entries.rbegin(); item != entries.rend(); ++item) {
            const auto seconds = static_cast<std::time_t>(item->timestamp_ms / 1000);
            const auto time = local_time(seconds);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(time.c_str());
            ImGui::TableNextColumn(); ImGui::TextUnformatted(item->source.c_str()); cyui::tooltip(item->source.c_str());
            ImGui::TableNextColumn(); ImGui::TextUnformatted(item->operation.c_str()); cyui::tooltip(item->operation.c_str());
            ImGui::TableNextColumn(); ImGui::TextUnformatted(item->status.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%.0f", item->elapsed_ms);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(item->detail.c_str()); cyui::tooltip(item->detail.c_str());
        }
        ImGui::EndTable();
    }

    void draw_footer() const {
        if (pending_.valid()) {
            cyui::spinner("##scan", 5);
            ImGui::SameLine();
            cyui::textDim("%s", tr("Scanning…").c_str());
        } else if (!error_.empty()) {
            cyui::textColoured(cyui::toneColour(Tone::error), "%s", tr("Scan failed").c_str());
            cyui::tooltip(error_.c_str());
        } else {
            cyui::textDim("%s  %s", tr("Last scan").c_str(), last_scan_.c_str());
        }
        if (!snapshot_.unreadable_sources.empty() || !snapshot_.processes_available || !snapshot_.ports_available) {
            ImGui::SameLine();
            if (cyui::ghostButton(trLabel("Scan details").c_str())) ImGui::OpenPopup("##scan-details");
        }
        ImGui::SameLine();
        const auto project = cymcp::path_utf8(options_.workspace);
        cyui::textFaint("%s", options_.workspace.filename().empty() ? project.c_str() :
                                  cymcp::path_utf8(options_.workspace.filename()).c_str());
        cyui::tooltip(project.c_str());

        if (ImGui::BeginPopup("##scan-details")) {
            if (!snapshot_.processes_available) wrapped(tr("The process inventory is incomplete."));
            if (!snapshot_.ports_available) wrapped(tr("The TCP inventory is incomplete."));
            if (!snapshot_.unreadable_sources.empty()) wrapped(tr("Some configuration files could not be read."));
            for (const auto& source : snapshot_.unreadable_sources) wrapped(cymcp::path_utf8(source));
            ImGui::EndPopup();
        }
    }
};
} // namespace

void draw_hub(WindowHandle window, Options options, const DrawFrame& frame) {
#ifdef _WIN32
    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    struct Uninitialize {
        HRESULT result;
        ~Uninitialize() { if (SUCCEEDED(result)) CoUninitialize(); }
    } uninitialize{com};
#endif
    register_translations();
    register_action_translations();
    register_assistant_translations();
    register_skill_translations();
    Window app(window, std::move(options));
    const Draw main = [&] { app.draw(); };
    const Draw assistant = [&] { app.draw_assistant_window(); };
    while (frame(main, assistant, app.assistant_open(), app.assistant_focus())) {}
}
} // namespace hub
