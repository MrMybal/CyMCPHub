// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "ui.hpp"
#include "action_panel.hpp"
#include "assistant_panel.hpp"
#include "autostart.hpp"
#include "resident_service.hpp"
#include "skill_panel.hpp"
#include "sdk_scan.hpp"
#include "platform.hpp"
#include "ui_kit.hpp"
#include "update_panel.hpp"

#include <cyui/Core.h>
#include <cyui/Icons.h>
#ifdef _WIN32
#include <shobjidl.h>
#include <wrl/client.h>
#else
#include <SDL3/SDL.h>
#endif

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <ctime>

namespace hub {
namespace {
using cyui::Tone;
using cyui::dp;
using cyui::tr;
using cyui::trLabel;
namespace icon = cyui::icon;

void register_translations() {
    cyui::registerTranslations("fr", {
        // Navigation and pages
        {"INVENTORY", "INVENTAIRE"}, {"LIBRARY", "BIBLIOTHÈQUE"}, {"MCP servers", "Serveurs MCP"},
        {"Skills", "Skills"}, {"Journal", "Journal"}, {"Settings", "Réglages"}, {"Updates", "Mises à jour"},
        {"AI setup", "Connexion IA"}, {"Assistant", "Assistant"},
        {"Servers declared by your AI clients and found on this computer.",
         "Serveurs déclarés par vos clients IA et repérés sur cet ordinateur."},
        {"Applications and folders that embed CyMCPSDK.", "Applications et dossiers qui intègrent CyMCPSDK."},
        {"Command-line tools installed on this computer.", "Outils en ligne de commande installés sur cet ordinateur."},
        {"Skills collected from connectors and projects.", "Skills collectés depuis les connecteurs et les projets."},
        {"Inventory scans, connector calls and assistant activity.",
         "Inventaires, appels aux connecteurs et activité de l’assistant."},
        {"Application, inventory sources and MCP service.", "Application, sources de l’inventaire et service MCP."},
        {"Unsaved changes", "Modifications non enregistrées"},
        // Header actions
        {"Sources", "Sources"}, {"Refresh", "Actualiser"}, {"Analyze folder…", "Analyser un dossier…"},
        {"Analyze file…", "Analyser un fichier…"}, {"Copy journal", "Copier le journal"}, {"Clear", "Effacer"},
        {"Save", "Enregistrer"}, {"Discard", "Annuler"},
        // Inventory
        {"Local MCP inventory", "Inventaire MCP local"}, {"Servers", "Serveurs"}, {"Verified", "Confirmés"},
        {"Processes", "Processus"}, {"Need attention", "À vérifier"}, {"All", "Tous"}, {"Active", "Actifs"},
        {"Attention", "À vérifier"}, {"Search servers", "Rechercher un serveur"},
        {"No matching servers", "Aucun serveur trouvé"}, {"Scanning…", "Inventaire en cours…"},
        {"Try another search or add a config file from Sources.", "Modifiez la recherche ou ajoutez un fichier via Sources."},
        {"Select a server", "Sélectionnez un serveur"},
        {"See its connection details and add an annotation.", "Consultez ses données de connexion et ajoutez une annotation."},
        {"Verified MCP", "MCP confirmé"}, {"Process observed", "Processus repéré"}, {"Configured", "Configuré"},
        {"Authentication required", "Authentification requise"}, {"Unverified", "Non confirmé"}, {"Disabled", "Désactivé"},
        {"Local TCP", "TCP local"}, {"Custom config", "Configuration ajoutée"}, {"CONNECTORS", "CONNECTEURS"},
        {"connectors", "connecteurs"}, {"active", "actifs"},
        {"Connection", "Connexion"}, {"Annotation", "Annotation"}, {"Source", "Source"}, {"File", "Fichier"},
        {"Transport", "Transport"}, {"Identity", "Identité"}, {"Protocol", "Protocole"}, {"Process", "Processus"},
        {"Instructions", "Instructions"}, {"Connection data", "Données de connexion"}, {"Copy JSON", "Copier le JSON"},
        {"Copied", "Copié"}, {"Save annotation", "Enregistrer l’annotation"}, {"Annotation saved", "Annotation enregistrée"},
        {"Could not save the annotation", "Impossible d’enregistrer l’annotation"},
        {"Annotation changed on disk. Saving will replace it.", "L’annotation a changé sur le disque. Enregistrer remplacera cette version."},
        {"Notes are shared with the AI tools on this computer.", "Les notes sont partagées avec les outils IA sur cet ordinateur."},
        {"Connection metadata omits configured secret values.", "Les valeurs des secrets configurés restent dans les fichiers source."},
        {"Open in Assistant", "Ouvrir dans l’assistant"},
        // Sources
        {"Choose a project folder…", "Choisir un dossier de projet…"}, {"Add a config file…", "Ajouter un fichier de configuration…"},
        {"Remove the extra config", "Retirer la configuration ajoutée"}, {"Probe local TCP ports", "Rechercher sur les ports TCP locaux"},
        // Software
        {"SDK inventory", "Inventaire SDK"}, {"CLI inventory", "Inventaire CLI"}, {"Groups", "Groupes"},
        {"Results", "Résultats"}, {"Files inspected", "Fichiers inspectés"}, {"Search SDKs", "Rechercher un SDK"},
        {"Search CLIs", "Rechercher une CLI"}, {"Installed CLIs", "CLI installées"},
        {"Include installed CLIs", "Inclure les CLI installées"}, {"Analyzed target", "Cible analysée"},
        {"Back to installed CLIs", "Revenir aux CLI installées"}, {"No file or folder selected", "Aucun fichier ou dossier sélectionné"},
        {"Path", "Chemin"}, {"Evidence", "Indices"}, {"Type", "Type"}, {"Variants", "Variantes"}, {"VARIANTS", "VARIANTES"},
        {"Copy path", "Copier le chemin"}, {"SDK binary", "Binaire avec indices SDK"}, {"SDK library", "Bibliothèque SDK"},
        {"SDK reference", "Référence au SDK"}, {"Known executable name", "Nom d’exécutable connu"},
        {"CLI install directory", "Dossier d’installation CLI"}, {"SDK library filename", "Nom de bibliothèque SDK"},
        {"Software scan in progress…", "Analyse des fichiers en cours…"}, {"No SDK detected", "Aucun SDK détecté"},
        {"No CLI detected", "Aucune CLI détectée"}, {"Select a result", "Sélectionnez un résultat"},
        {"See its path and detection evidence.", "Consultez son chemin et les indices relevés."},
        {"Choose an application or a folder to look for CyMCPSDK markers and known CLIs.",
         "Choisissez une application ou un dossier pour rechercher des indices CyMCPSDK et des CLI connues."},
        {"The scan stopped early. Some results may be missing.", "L’analyse s’est arrêtée avant la fin. Des résultats peuvent manquer."},
        {"Unreadable files", "Fichiers illisibles"}, {"skills found in this target.", "skills trouvés dans cette cible."},
        {"Open the skill library", "Ouvrir la bibliothèque de skills"},
        // Journal
        {"Time", "Heure"}, {"Operation", "Opération"}, {"Status", "Statut"}, {"Details", "Détails"},
        {"Search the journal", "Rechercher dans le journal"}, {"Problems", "Problèmes"},
        {"The journal is empty", "Le journal est vide"},
        {"Inventory scans and connector activity appear here.", "Les inventaires et l’activité des connecteurs apparaissent ici."},
        {"No matching entries", "Aucune entrée correspondante"},
        {"inventory/mcp", "Inventaire MCP"}, {"inventory/software", "Inventaire SDK et CLI"},
        {"inventory/initialize", "Sonde MCP"}, {"Access denied", "Accès refusé"}, {"Started", "Démarré"},
        {"Partial", "Partiel"}, {"Error", "Erreur"},
        {"Local TCP candidates", "Adresses TCP à vérifier"}, {"MCP discovered via TCP", "MCP découverts via TCP"},
        {"Local TCP discovery disabled", "Recherche sur les ports TCP locaux désactivée"},
        {"Unreadable config files", "Configurations illisibles"}, {"Catalog entries", "Entrées au catalogue"},
        // Status bar
        {"MCP service", "Service MCP"}, {"MCP service running", "Service MCP actif"}, {"MCP service stopped", "Service MCP arrêté"},
        {"Last scan", "Dernier inventaire"}, {"Scan failed", "Échec de l’inventaire"}, {"Incomplete scan", "Inventaire incomplet"},
        {"Scan details", "Détails de l’inventaire"}, {"Some configuration files could not be read.", "Certains fichiers de configuration sont illisibles."},
        {"The process inventory is incomplete.", "L’inventaire des processus est incomplet."},
        {"The TCP inventory is incomplete.", "L’inventaire TCP est incomplet."}, {"Project folder", "Dossier du projet"},
        // Settings
        {"General", "Général"}, {"Application", "Application"}, {"Settings saved", "Réglages enregistrés"},
        {"Inventory sources", "Sources de l’inventaire"}, {"Extra configuration", "Configuration supplémentaire"},
        {"No extra configuration", "Aucune configuration supplémentaire"}, {"Change…", "Modifier…"}, {"Add…", "Ajouter…"},
        {"Remove", "Retirer"}, {"Client configuration", "Configuration client"},
        {"Start the MCP service to copy the HTTP configuration.", "Démarrez le service MCP pour copier la configuration HTTP."},
        {"Automatic refresh", "Actualisation automatique"},
        {"How often the MCP inventory runs again in the background.", "Fréquence de l’inventaire MCP en arrière-plan."},
        {"Off", "Désactivée"}, {"15 seconds", "15 secondes"}, {"30 seconds", "30 secondes"},
        {"1 minute", "1 minute"}, {"2 minutes", "2 minutes"}, {"5 minutes", "5 minutes"},
        {"Start at login", "Démarrer à l’ouverture de session"}, {"Start with Windows", "Démarrer avec Windows"},
        {"Could not change automatic startup.", "Impossible de modifier le démarrage automatique."},
        {"Local TCP ports", "Ports TCP locaux"},
        {"Look for MCP servers listening on local TCP ports.", "Recherche des serveurs MCP qui écoutent sur les ports TCP locaux."},
        {"Start with the app", "Démarrer avec l’application"}, {"Service port", "Port du service"}, {"Access mode", "Mode d’accès"},
        {"Token required", "Jeton requis"}, {"Free (local)", "Libre (local)"},
        {"Any local client can use the tools without a token.", "Tous les clients locaux peuvent utiliser les outils sans jeton."},
        {"Save the settings before starting or stopping the service.", "Enregistrez les réglages avant de démarrer ou d’arrêter le service."},
        {"Cannot read app settings.", "Impossible de lire les réglages."}, {"Cannot save app settings.", "Impossible d’enregistrer les réglages."},
        {"The MCP port must be between 1 and 65535.", "Le port MCP doit être compris entre 1 et 65535."},
        {"Invalid refresh interval.", "Intervalle d’actualisation invalide."},
        {"MCP service (HTTP)", "Service MCP (HTTP)"}, {"Start MCP service", "Démarrer le service MCP"},
        {"Stop MCP service", "Arrêter le service MCP"}, {"Token hidden", "Jeton masqué"},
        {"Copy configuration", "Copier la configuration"}, {"The copied configuration includes the token.", "La configuration copiée inclut le jeton."},
        {"Add this entry to the MCP configuration of your AI client.", "Ajoutez cette entrée à la configuration MCP de votre client IA."},
        {"The AI can list servers, read connection details, edit annotations and scan for SDKs and CLIs.",
         "L’IA peut lister les serveurs, lire les données de connexion, modifier les annotations et rechercher le SDK et les CLI."}
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

bool needs_attention(Status state) { return state == Status::authentication || state == Status::unverified; }

Tone journal_tone(const std::string& status) {
    if (status == "OK") return Tone::success;
    if (status == "Error" || status == "error") return Tone::error;
    if (status == "Partial" || status == "Access denied" || status == "warning") return Tone::warning;
    if (status == "Started") return Tone::info;
    return Tone::neutral;
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

std::string leaf_name(const fs::path& path) {
    const auto leaf = path.filename();
    return cymcp::path_utf8(leaf.empty() ? path : leaf);
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

struct Draft { std::string original, text; };
struct Group {
    std::string key, name;
    std::vector<Server*> variants;
};

struct SoftwareGroup {
    std::string key, name;
    std::vector<const SoftwareFinding*> variants;
};

enum View { mcp, sdk, cli, skills_view, journal, settings_view };
constexpr const char* view_labels[] = {"MCP servers", "SDK", "CLI", "Skills", "Journal", "Settings"};
constexpr auto mcp_inventory = "inventory/mcp";
constexpr auto software_inventory = "inventory/software";
enum ListFilter { all_servers, active_servers, attention_servers };

std::string inventory_count(const char* label, std::size_t count) {
    return tr(label) + " " + std::to_string(count);
}

double elapsed_ms(cymcp::Clock::time_point started) {
    return std::chrono::duration<double, std::milli>(cymcp::Clock::now() - started).count();
}

// Chips that wrap onto new lines when the row is full.
class Chips {
    const float end_ = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    bool first_ = true;

public:
    bool draw(const std::string& label, const std::string& id, bool active) {
        const auto width = std::min(ImGui::GetContentRegionAvail().x, cyui::buttonWidth(label.c_str()));
        if (!first_ && ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + width <= end_) ImGui::SameLine();
        first_ = false;
        const auto control = label + "###" + id;
        return cyui::button(control.c_str(), active ? cyui::Variant::primary : cyui::Variant::outline, ImVec2(width, 0));
    }
};

const char* software_type(const std::string& kind) {
    if (kind == "sdk_binary") return "SDK binary";
    if (kind == "sdk_library") return "SDK library";
    if (kind == "sdk_reference") return "SDK reference";
    return "CLI";
}

bool contains_text(const std::string& value, const std::string& query) {
    return query.empty() || group_key(value).find(query) != std::string::npos;
}

class Window {
    enum class Pick { workspace, configuration, software_folder, software_file };
    WindowHandle window_;
    Options options_;
    AppSettings settings_, settings_draft_;
    std::string settings_error_;
    bool login_saved_ = false, login_draft_ = false;
    Snapshot snapshot_;
    std::future<Snapshot> pending_;
    std::future<fs::path> pending_path_;
    Pick pick_ = Pick::workspace;
    cymcp::Cancellation cancellation_;
    SoftwareScan software_;
    std::future<SoftwareScan> pending_software_;
    cymcp::Cancellation software_cancellation_;
    cymcp::Clock::time_point scan_started_, scan_finished_, software_scan_started_;
    fs::path software_target_;
    std::string error_, last_scan_, selected_group_, software_error_;
    std::array<char, 256> search_{};
    std::array<char, 256> journal_search_{};
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
    UpdatePanel updates_;
    Json current_context_ = Json::object();
    bool include_clis_ = true, software_scanned_ = false;
    int view_ = mcp;
    int list_filter_ = all_servers;
    int journal_filter_ = 0;
    int setup_transport_ = 0;
    bool assistant_open_ = false, assistant_focus_ = false;
    bool setup_open_ = false, updates_open_ = false;
    float nav_footer_height_ = 0;

public:
    Window(WindowHandle window, Options options) : window_(window), options_(std::move(options)) {
        try { settings_ = read_app_settings(); }
        catch (const std::exception& error) {
            settings_error_ = tr("Cannot read app settings.");
            journal_->add("CyMCPHub", "settings/load", "Error", error.what());
        }
        options_.scan_ports = options_.scan_ports && settings_.scan_ports;
        reset_settings_draft();
        resident_.set_running(settings_.service.start_on_launch, options_, settings_.service);
        refresh();
    }
    ~Window() { cancellation_.cancel(); software_cancellation_.cancel(); }

    void tick() {
        poll();
        poll_software();
        if (settings_.refresh_seconds > 0 && !pending_.valid() && !pending_path_.valid() &&
            cymcp::Clock::now() - scan_finished_ >= std::chrono::seconds(settings_.refresh_seconds))
            refresh();
    }

    void draw() {
        poll_path();
        assistant_.poll();
        skills_.poll();
        if (tray_assistant_requested) {
            tray_assistant_requested = false;
            open_assistant();
        }
        current_context_ = Json::object();
        current_context_["tab"] = view_labels[view_];
        const auto& palette = cyui::palette();
        const auto* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        {
            const cyui::ScopedVar padding(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::Begin("##hub", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                                           ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus);
        }
        const auto size = ImGui::GetContentRegionAvail();
        const auto status_height = ImGui::GetFontSize() + dp(14);
        const auto body_height = std::max(dp(120), size.y - status_height);
        const auto nav_width = dp(216);
        auto* draw = ImGui::GetWindowDrawList();
        const auto origin = ImGui::GetCursorScreenPos();
        {
            const cyui::ScopedColor ground(ImGuiCol_ChildBg, palette.bg0);
            const cyui::ScopedVar padding(ImGuiStyleVar_WindowPadding, dp(12, 16));
            if (ImGui::BeginChild("##nav", ImVec2(nav_width, body_height), ImGuiChildFlags_AlwaysUseWindowPadding,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) draw_nav();
            ImGui::EndChild();
        }
        ImGui::SameLine(0, 0);
        {
            const cyui::ScopedVar padding(ImGuiStyleVar_WindowPadding, dp(24, 20));
            if (ImGui::BeginChild("##page", ImVec2(0, body_height), ImGuiChildFlags_AlwaysUseWindowPadding,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) draw_page();
            ImGui::EndChild();
        }
        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + body_height));
        {
            const cyui::ScopedColor ground(ImGuiCol_ChildBg, palette.bg0);
            const cyui::ScopedVar padding(ImGuiStyleVar_WindowPadding, ImVec2(dp(14), (status_height - ImGui::GetFontSize() * 0.86f) * 0.5f));
            if (ImGui::BeginChild("##status", ImVec2(0, status_height), ImGuiChildFlags_AlwaysUseWindowPadding,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) draw_status_bar();
            ImGui::EndChild();
        }
        const auto seam = std::max(1.0f, std::floor(dp(1)));
        draw->AddLine(ImVec2(origin.x + nav_width - seam * 0.5f, origin.y), ImVec2(origin.x + nav_width - seam * 0.5f, origin.y + body_height),
                      palette.lineStrong, seam);
        draw->AddLine(ImVec2(origin.x, origin.y + body_height), ImVec2(origin.x + size.x, origin.y + body_height), palette.lineStrong, seam);
        draw_setup();
        updates_.draw(window_, updates_open_);
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
        ImGui::SetNextWindowSize(cyui::dp(720, 640), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(main->WorkPos.x + cyui::dp(85), main->WorkPos.y + cyui::dp(62)), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(cyui::dp(480, 400), ImVec2(10000, 10000));
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
        const cyui::ScopedVar padding(ImGuiStyleVar_WindowPadding, dp(18, 16));
        if (ImGui::Begin(title.c_str(), &assistant_open_, flags)) assistant_.draw(options_);
        ImGui::End();
    }

private:
    void open_assistant() {
        assistant_open_ = true;
        assistant_focus_ = true;
    }

    // ---------------------------------------------------------------- scans

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

    void refresh() {
        if (pending_.valid()) return;
        resident_.sync_options(options_, settings_.service);
        error_.clear();
        cancellation_ = {};
        scan_started_ = cymcp::Clock::now();
        journal_->add("CyMCPHub", mcp_inventory, "Started");
        try {
            pending_ = std::async(std::launch::async, [options = options_, cancel = cancellation_, log = journal_] {
                return scan(options, cancel, log.get());
            });
        } catch (const std::exception& error) {
            record_scan_error(mcp_inventory, scan_started_, error, error_);
            scan_finished_ = cymcp::Clock::now();
        }
    }

    void poll() {
        if (!pending_.valid() || pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        scan_finished_ = cymcp::Clock::now();
        try {
            snapshot_ = pending_.get();
            last_scan_ = local_time(std::time(nullptr));
            const auto discovered = std::count_if(snapshot_.servers.begin(), snapshot_.servers.end(),
                [](const Server& server) { return server.source_file.empty() && server.status == Status::verified; });
            auto detail = inventory_count("Catalog entries", snapshot_.servers.size());
            if (options_.scan_ports) {
                detail += " · " + inventory_count("Local TCP candidates", snapshot_.tcp_candidates);
                detail += " · " + inventory_count("MCP discovered via TCP", static_cast<std::size_t>(discovered));
            } else {
                detail += " · " + tr("Local TCP discovery disabled");
            }
            if (!snapshot_.processes_available) detail += " · " + tr("The process inventory is incomplete.");
            if (!snapshot_.ports_available) detail += " · " + tr("The TCP inventory is incomplete.");
            if (!snapshot_.unreadable_sources.empty())
                detail += " · " + inventory_count("Unreadable config files", snapshot_.unreadable_sources.size());
            journal_->add("CyMCPHub", mcp_inventory, scan_incomplete() ? "Partial" : "OK", detail, elapsed_ms(scan_started_));
        } catch (const std::exception& error) {
            record_scan_error(mcp_inventory, scan_started_, error, error_);
        }
    }

    bool scan_incomplete() const {
        return !snapshot_.processes_available || !snapshot_.ports_available || !snapshot_.unreadable_sources.empty();
    }

    void refresh_software() {
        if (pending_software_.valid()) return;
        software_scanned_ = true;
        software_error_.clear();
        software_cancellation_ = {};
        software_scan_started_ = cymcp::Clock::now();
        journal_->add("CyMCPHub", software_inventory, "Started");
        try {
            pending_software_ = std::async(std::launch::async,
                [target = software_target_, include = include_clis_, cancel = software_cancellation_] {
                    return scan_software(target, include, cancel);
                });
        } catch (const std::exception& error) {
            record_scan_error(software_inventory, software_scan_started_, error, software_error_);
        }
    }

    void poll_software() {
        if (!pending_software_.valid() || pending_software_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        try {
            software_ = pending_software_.get();
            auto detail = inventory_count("Files inspected", software_.examined) +
                          " · " + inventory_count("Results", software_.findings.size());
            if (software_.unreadable) detail += " · " + inventory_count("Unreadable files", software_.unreadable);
            const bool partial = software_.partial || software_.unreadable;
            journal_->add("CyMCPHub", software_inventory, partial ? "Partial" : "OK", detail, elapsed_ms(software_scan_started_));
        } catch (const std::exception& error) {
            record_scan_error(software_inventory, software_scan_started_, error, software_error_);
        }
    }

    void record_scan_error(const char* operation, cymcp::Clock::time_point started,
                           const std::exception& error, std::string& displayed_error) {
        displayed_error = cymcp::redact_error_message(error.what());
        journal_->add("CyMCPHub", operation, "Error", displayed_error, elapsed_ms(started));
    }

    void show_view(int view) {
        if (view == settings_view && view_ != settings_view && !settings_dirty()) reset_settings_draft();
        view_ = view;
        if ((view_ == sdk || view_ == cli) && !software_scanned_) refresh_software();
    }

    // ---------------------------------------------------------------- shell

    void draw_nav() {
        const auto& palette = cyui::palette();
        {
            const cyui::ScopedFont brand(cyui::boldFont(), 1.25f);
            ImGui::TextUnformatted("CyMCPHub");
        }
        ImGui::SameLine(0, dp(6));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetFontSize() * 0.18f);
        cyui::textFaint("v%s", CYMCP_HUB_VERSION);
        cyui::space(10);

        const auto groups = all_groups();
        const auto count = [](std::size_t value) { return value ? std::to_string(value) : std::string{}; };
        const auto item = [&](const char* glyph, const char* label, int view, const std::string& badge) {
            const auto text = tr(label);
            if (cyui::sidebarItem(glyph, text.c_str(), view_ == view, badge.empty() ? nullptr : badge.c_str())) show_view(view);
        };
        cyui::sidebarGroup(tr("INVENTORY").c_str());
        item(icon::connect, "MCP servers", mcp, count(groups.size()));
        const auto software_count = [&](bool cli_kind) {
            return static_cast<std::size_t>(std::count_if(software_.findings.begin(), software_.findings.end(),
                [&](const SoftwareFinding& finding) { return (finding.kind == "cli") == cli_kind; }));
        };
        item(icon::code, "SDK", sdk, count(software_count(false)));
        item(icon::keyboard, "CLI", cli, count(software_count(true)));
        cyui::sidebarGroup(tr("LIBRARY").c_str());
        item(icon::library, "Skills", skills_view, count(skills_.count()));
        item(icon::list, "Journal", journal, {});

        const auto window_bottom = ImGui::GetWindowPos().y + ImGui::GetWindowHeight() - ImGui::GetStyle().WindowPadding.y;
        const auto footer_top = window_bottom - nav_footer_height_;
        if (ImGui::GetCursorScreenPos().y < footer_top) ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, footer_top));
        const auto start = ImGui::GetCursorScreenPos().y;
        const auto assistant = tr("Assistant"), setup = tr("AI setup"), updates = tr("Updates");
        if (cyui::sidebarItem(icon::lightbulb, assistant.c_str(), assistant_open_)) open_assistant();
        if (cyui::sidebarItem(icon::link, setup.c_str(), setup_open_)) setup_open_ = true;
        if (cyui::sidebarItem(icon::download, updates.c_str(), updates_open_, updates_.available() ? "1" : nullptr))
            updates_open_ = true;
        item(icon::settings, "Settings", settings_view, {});
        nav_footer_height_ = ImGui::GetCursorScreenPos().y - start;
        (void)palette;
    }

    void draw_status_bar() {
        const auto running = resident_.running();
        const auto line = ImGui::GetFontSize() * 0.86f;
        kit::dot(cyui::toneColour(running ? Tone::success : Tone::neutral), line);
        ImGui::SameLine(0, dp(2));
        if (running) cyui::statusText(Tone::neutral, "%s  %s", tr("MCP service").c_str(), mcp_service_url(settings_.service.port).c_str());
        else cyui::statusText(Tone::neutral, "%s", tr("MCP service stopped").c_str());
        if (ImGui::IsItemClicked()) show_view(settings_view);

        ImGui::SameLine(0, dp(22));
        if (pending_.valid()) {
            cyui::spinner("##scan", line * 0.4f);
            ImGui::SameLine(0, dp(6));
            cyui::statusText(Tone::neutral, "%s", tr("Scanning…").c_str());
        } else if (!error_.empty()) {
            cyui::statusText(Tone::error, "%s", tr("Scan failed").c_str());
            cyui::tooltip(error_.c_str());
        } else if (!last_scan_.empty()) {
            cyui::statusText(Tone::neutral, "%s  %s", tr("Last scan").c_str(), last_scan_.c_str());
        }
        if (scan_incomplete() && !pending_.valid()) {
            ImGui::SameLine(0, dp(16));
            cyui::statusText(Tone::warning, "%s", tr("Incomplete scan").c_str());
            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            if (ImGui::IsItemClicked()) ImGui::OpenPopup("##scan-details");
            cyui::tooltip(tr("Scan details").c_str());
        }
        if (pending_software_.valid()) {
            ImGui::SameLine(0, dp(22));
            cyui::spinner("##software-scan", line * 0.4f);
            ImGui::SameLine(0, dp(6));
            cyui::statusText(Tone::neutral, "%s", tr("Software scan in progress…").c_str());
        }

        const auto project = cymcp::path_utf8(options_.workspace);
        const auto leaf = (cyui::iconsAvailable() ? std::string(icon::folder) + "  " : std::string{}) + leaf_name(options_.workspace);
        {
            const cyui::ScopedFont reduced(nullptr, 0.86f);
            ImGui::SameLine();
            cyui::alignRight(ImGui::CalcTextSize(leaf.c_str()).x);
        }
        cyui::statusText(Tone::neutral, "%s", leaf.c_str());
        cyui::tooltip((tr("Project folder") + "\n" + project).c_str());

        if (ImGui::BeginPopup("##scan-details")) {
            cyui::heading(tr("Scan details").c_str(), 3);
            if (!snapshot_.processes_available) kit::wrapped(tr("The process inventory is incomplete."));
            if (!snapshot_.ports_available) kit::wrapped(tr("The TCP inventory is incomplete."));
            if (!snapshot_.unreadable_sources.empty()) kit::wrapped(tr("Some configuration files could not be read."));
            for (const auto& source : snapshot_.unreadable_sources) kit::wrapped_dim(cymcp::path_utf8(source));
            ImGui::EndPopup();
        }
    }

    void draw_page() {
        switch (view_) {
        case mcp: draw_mcp_page(); break;
        case sdk: case cli: draw_software_page(); break;
        case skills_view: draw_skills_page(); break;
        case journal: draw_journal_page(); break;
        case settings_view: draw_settings_page(); break;
        default: break;
        }
    }

    template <class List, class Details>
    void draw_split(List list, Details details) {
        const auto available = ImGui::GetContentRegionAvail();
        const float left = std::clamp(available.x * 0.34f, dp(270), dp(400));
        const float height = std::max(available.y, dp(120));
        if (kit::begin_pane("##list-pane", ImVec2(left, height), false)) list();
        kit::end_pane();
        ImGui::SameLine(0, dp(14));
        if (kit::begin_pane("##detail-pane", ImVec2(0, height))) details();
        kit::end_pane();
    }

    // ---------------------------------------------------------------- MCP servers

    std::vector<Group> all_groups() {
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
        return groups;
    }

    bool passes_filter(const Server& server) const {
        if (list_filter_ == active_servers && !is_active(server.status)) return false;
        if (list_filter_ == attention_servers && !needs_attention(server.status)) return false;
        return matches(server, search_.data());
    }

    void draw_mcp_page() {
        const bool busy = pending_.valid() || pending_path_.valid();
        const std::array actions{
            kit::Action{icon::folder, trLabel("Sources"), cyui::Variant::secondary, true, {}},
            kit::Action{icon::refresh, trLabel("Refresh"), cyui::Variant::primary, !busy, {}}};
        const auto clicked = kit::page_header(tr("MCP servers").c_str(),
                                              tr("Servers declared by your AI clients and found on this computer."), actions);
        if (clicked == 0) ImGui::OpenPopup("##sources");
        if (clicked == 1) refresh();
        draw_sources_popup();
        if (!error_.empty()) cyui::callout(Tone::error, (tr("Scan failed") + "  ·  " + error_).c_str());

        auto groups = all_groups();
        const auto count_groups = [&](auto predicate) {
            return static_cast<std::size_t>(std::count_if(groups.begin(), groups.end(), [&](const Group& group) {
                return std::any_of(group.variants.begin(), group.variants.end(), [&](const Server* server) { return predicate(*server); });
            }));
        };
        const std::array stats{
            kit::Stat{"Servers", groups.size(), Tone::accent},
            kit::Stat{"Verified", count_groups([](const Server& server) { return server.status == Status::verified; }), Tone::success},
            kit::Stat{"Processes", count_groups([](const Server& server) { return server.status == Status::process_seen; }), Tone::info},
            kit::Stat{"Need attention", count_groups([](const Server& server) { return needs_attention(server.status); }), Tone::warning}};
        kit::stat_tiles("##mcp-stats", stats);

        std::erase_if(groups, [&](const Group& group) {
            return std::none_of(group.variants.begin(), group.variants.end(), [&](const Server* server) { return passes_filter(*server); });
        });
        draw_split([&] { draw_server_list(groups); }, [&] { draw_server_details(groups); });
    }

    void draw_sources_popup() {
        if (!ImGui::BeginPopup("##sources")) return;
        {
            const cyui::ScopedDisabled scanning(pending_.valid() || pending_path_.valid());
            if (ImGui::MenuItem(kit::with_icon(icon::folder, trLabel("Choose a project folder…")).c_str())) pick_path(Pick::workspace);
            if (ImGui::MenuItem(kit::with_icon(icon::openFile, trLabel("Add a config file…")).c_str())) pick_path(Pick::configuration);
            if (!options_.extra_config.empty() &&
                ImGui::MenuItem(kit::with_icon(icon::remove, trLabel("Remove the extra config")).c_str())) {
                options_.extra_config.clear();
                refresh();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(trLabel("Probe local TCP ports").c_str(), nullptr, options_.scan_ports)) {
                auto settings = settings_;
                settings.scan_ports = !options_.scan_ports;
                if (!apply_settings(settings)) cyui::notify(settings_error_.c_str(), Tone::error);
            }
        }
        ImGui::EndPopup();
    }

    void draw_server_list(const std::vector<Group>& groups) {
        ImGui::Indent(dp(8));
        cyui::space(6);
        cyui::searchField("##search", search_.data(), static_cast<int>(search_.size()), tr("Search servers").c_str(), -dp(8));
        static constexpr const char* filters[] = {"All", "Active", "Attention"};
        ImGui::SetCursorPosX(ImGui::GetCursorPosX());
        cyui::segmented("##server-filter", filters, 3, &list_filter_, ImGui::GetContentRegionAvail().x - dp(8));
        ImGui::Unindent(dp(8));
        cyui::space(4);

        if (groups.empty()) {
            selected_group_.clear();
            if (pending_.valid()) kit::empty_state(tr("Scanning…"), {}, icon::search);
            else kit::empty_state(tr("No matching servers"), tr("Try another search or add a config file from Sources."), icon::connect);
            return;
        }
        if (std::none_of(groups.begin(), groups.end(), [&](const Group& item) { return item.key == selected_group_; }))
            selected_group_ = groups.front().key;

        if (kit::begin_rows("##rows")) {
            for (const auto& group : groups) {
                const auto& server = *group.variants.front();
                const auto active = std::count_if(group.variants.begin(), group.variants.end(),
                                                  [](const Server* item) { return is_active(item->status); });
                const auto best = std::min_element(group.variants.begin(), group.variants.end(), [](const Server* a, const Server* b) {
                    return static_cast<int>(a->status) < static_cast<int>(b->status);
                });
                const auto subtitle = group.variants.size() > 1 ?
                    std::to_string(group.variants.size()) + " " + tr("connectors") + "  ·  " + std::to_string(active) + " " + tr("active") :
                    source_name(server) + "  ·  " + server.transport + "  ·  " + tr(status_label(server.status));
                if (kit::list_row(group.key.c_str(), group.name, subtitle, cyui::toneColour(status_tone((*best)->status)),
                                  group.variants.size() > 1 ? std::to_string(group.variants.size()) : std::string{},
                                  selected_group_ == group.key)) selected_group_ = group.key;
            }
        }
        kit::end_rows();
    }

    Server& selected_variant(const Group& group) {
        const auto& selected_id = selected_connectors_[group.key];
        const auto current = std::find_if(group.variants.begin(), group.variants.end(), [&](const Server* server) {
            return server->id == selected_id;
        });
        return current == group.variants.end() ? *group.variants.front() : **current;
    }

    void draw_server_details(const std::vector<Group>& groups) {
        const auto group = std::find_if(groups.begin(), groups.end(), [&](const Group& item) { return item.key == selected_group_; });
        if (group == groups.end()) {
            if (pending_.valid()) kit::empty_state(tr("Scanning…"), {}, icon::search);
            else kit::empty_state(tr("Select a server"), tr("See its connection details and add an annotation."), icon::connect);
            return;
        }
        auto& server = selected_variant(*group);
        current_context_["selection"] = group->name;
        const cyui::ScopedID group_id(group->key.c_str());

        const auto assistant = kit::with_icon(icon::lightbulb, trLabel("Open in Assistant"));
        cyui::heading(group->name.c_str(), 2);
        ImGui::SameLine();
        cyui::alignRight(cyui::buttonWidth(assistant.c_str()));
        {
            const cyui::ScopedDisabled disabled(server.status == Status::disabled);
            if (cyui::button(assistant.c_str())) {
                assistant_.attach(server);
                open_assistant();
            }
        }
        cyui::badge(tr(status_label(server.status)).c_str(), status_tone(server.status));
        ImGui::SameLine();
        cyui::badge(server.transport.c_str());
        ImGui::SameLine();
        cyui::badge(source_name(server).c_str());
        if (server.pid) {
            ImGui::SameLine();
            cyui::textFaint("PID %lu", server.pid);
        }

        if (group->variants.size() > 1) {
            cyui::space(6);
            cyui::overline((tr("CONNECTORS") + "  " + std::to_string(group->variants.size())).c_str(), cyui::palette().textFaint);
            Chips chips;
            std::map<std::string, int> seen_labels;
            auto& selected_id = selected_connectors_[group->key];
            for (auto* variant : group->variants) {
                auto label = source_name(*variant) + " · " + variant->transport;
                const auto duplicate = ++seen_labels[label];
                if (duplicate > 1) label += " #" + std::to_string(duplicate);
                if (chips.draw(label, variant->id, variant == &server)) selected_id = variant->id;
                if (!variant->source_file.empty()) label += '\n' + cymcp::path_utf8(variant->source_file);
                cyui::tooltip(label.c_str());
            }
        }
        cyui::space(4);
        draw_variant(server);
    }

    void draw_variant(Server& server) {
        const cyui::ScopedID id(server.id.c_str());
        current_context_["connectorId"] = server.id;
        current_context_["connectorSource"] = server.source;
        current_context_["transport"] = server.transport;
        current_context_["status"] = status_key(server.status);
        const auto& palette = cyui::palette();
        cyui::ScopedColor tabs(ImGuiCol_TabSelected, palette.bg3);
        tabs.push(ImGuiCol_TabDimmedSelected, palette.bg3)
            .push(ImGuiCol_TabSelectedOverline, palette.accent)
            .push(ImGuiCol_TabDimmedSelectedOverline, palette.accent);
        if (!ImGui::BeginTabBar("##detail-tabs", ImGuiTabBarFlags_DrawSelectedOverline)) return;
        if (ImGui::BeginTabItem(trLabel("Connection").c_str())) {
            current_context_["detailTab"] = "Connection";
            draw_tab_body([&] { draw_connection(server); });
            ImGui::EndTabItem();
        }
        const auto annotation = tr("Annotation") + (server.note.empty() ? "" : "  •") + "###annotation";
        if (ImGui::BeginTabItem(annotation.c_str())) {
            current_context_["detailTab"] = "Annotation";
            draw_tab_body([&] { draw_annotation(server); });
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(trLabel("Tools").c_str())) {
            current_context_["detailTab"] = "Tools";
            draw_tab_body([&] { actions_.draw(server, options_, false); });
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(trLabel("Resources").c_str())) {
            current_context_["detailTab"] = "Resources";
            draw_tab_body([&] { actions_.draw(server, options_, true); });
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(trLabel("Skills").c_str())) {
            current_context_["detailTab"] = "Skills";
            draw_tab_body([&] {
                current_context_["skill"] = skills_.draw_connector(server, options_, [&](const Server& connector) {
                    assistant_.draft_skill(connector);
                    open_assistant();
                });
            });
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    template <class Body>
    static void draw_tab_body(Body body) {
        const cyui::ScopedColor ground(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
        if (ImGui::BeginChild("##tab-body", ImVec2(0, 0))) {
            cyui::space(6);
            body();
        }
        ImGui::EndChild();
    }

    void draw_connection(const Server& server) {
        if (cyui::beginProperties("##metadata", 112)) {
            cyui::property(tr("Source").c_str());
            kit::wrapped(source_name(server));
            if (!server.source_file.empty()) {
                cyui::property(tr("File").c_str());
                kit::wrapped_dim(cymcp::path_utf8(server.source_file));
            }
            cyui::property(tr("Transport").c_str());
            kit::wrapped(server.transport);
            if (!server.server_name.empty()) {
                cyui::property(tr("Identity").c_str());
                kit::wrapped(server.version.empty() ? server.server_name : server.server_name + "  " + server.version);
            }
            if (!server.protocol.empty()) {
                cyui::property(tr("Protocol").c_str());
                kit::wrapped(server.protocol);
            }
            if (server.pid) {
                cyui::property(tr("Process").c_str());
                kit::wrapped("PID " + std::to_string(server.pid));
            }
            if (!server.summary.empty()) {
                cyui::property(tr("Instructions").c_str());
                kit::wrapped_dim(server.summary);
            }
            cyui::endProperties();
        }
        cyui::space(10);
        const auto json = server.connection.dump(2);
        if (kit::section_header(tr("Connection data").c_str(), icon::copy, tr("Copy JSON").c_str())) kit::copy_to_clipboard(json);
        kit::code_box("##connection-data", json, 170);
        if (!server.source_file.empty()) cyui::textFaint("%s", tr("Connection metadata omits configured secret values.").c_str());
    }

    void draw_annotation(Server& server) {
        auto& draft = drafts_.try_emplace(server.id, Draft{server.note, server.note}).first->second;
        if (draft.text == draft.original && draft.original != server.note) draft = {server.note, server.note};
        kit::wrapped_dim(tr("Notes are shared with the AI tools on this computer."));
        cyui::space(2);
        const cyui::ScopedID id(server.id.c_str());
        std::array<char, 8193> text{};
        std::copy_n(draft.text.begin(), std::min(draft.text.size(), text.size() - 1), text.begin());
        if (ImGui::InputTextMultiline("##annotation-text", text.data(), text.size(), ImVec2(-FLT_MIN, dp(180))))
            draft.text = text.data();
        if (draft.original != server.note) cyui::callout(Tone::warning, tr("Annotation changed on disk. Saving will replace it.").c_str());
        const cyui::ScopedDisabled disabled(draft.text == server.note);
        if (cyui::primaryButton(kit::with_icon(icon::save, trLabel("Save annotation")).c_str())) {
            try {
                set_note(server.id, draft.text);
                server.note = draft.original = draft.text;
                cyui::notify(tr("Annotation saved").c_str(), Tone::success);
            } catch (const std::exception&) {
                cyui::notify(tr("Could not save the annotation").c_str(), Tone::error);
            }
        }
    }

    // ---------------------------------------------------------------- SDK and CLI

    void draw_software_page() {
        const bool cli_view = view_ == cli;
        const bool busy = pending_software_.valid() || pending_path_.valid();
        const cyui::ScopedID view_id(view_);
        const auto subtitle = software_target_.empty() ?
            tr(cli_view ? "Command-line tools installed on this computer." : "Applications and folders that embed CyMCPSDK.") :
            cymcp::path_utf8(software_target_);
        const std::array actions{
            kit::Action{icon::folder, trLabel("Analyze folder…"), cyui::Variant::secondary, !busy, {}},
            kit::Action{icon::openFile, trLabel("Analyze file…"), cyui::Variant::secondary, !busy, {}},
            kit::Action{icon::refresh, trLabel("Refresh"), cyui::Variant::primary, !busy, {}}};
        switch (kit::page_header(cli_view ? "CLI" : "SDK", subtitle, actions)) {
        case 0: pick_path(Pick::software_folder); break;
        case 1: pick_path(Pick::software_file); break;
        case 2: refresh_software(); break;
        default: break;
        }

        if (!software_target_.empty()) {
            const cyui::ScopedDisabled disabled(busy);
            if (cyui::ghostButton(kit::with_icon(icon::back, trLabel("Back to installed CLIs")).c_str())) {
                software_target_.clear();
                include_clis_ = true;
                refresh_software();
            }
            ImGui::SameLine(0, dp(16));
            if (ImGui::Checkbox(trLabel("Include installed CLIs").c_str(), &include_clis_)) refresh_software();
            cyui::space(4);
        }
        if (!software_error_.empty()) cyui::callout(Tone::error, (tr("Scan failed") + "  ·  " + software_error_).c_str());
        if (software_.partial || software_.unreadable) {
            auto message = tr("The scan stopped early. Some results may be missing.");
            if (!software_.partial) message.clear();
            if (software_.unreadable) message += (message.empty() ? "" : "  ") + inventory_count("Unreadable files", software_.unreadable);
            cyui::callout(Tone::warning, message.c_str());
        }
        if (!software_.skills.empty()) {
            cyui::callout(Tone::info, (std::to_string(software_.skills.size()) + " " + tr("skills found in this target.")).c_str());
            if (cyui::ghostButton(kit::with_icon(icon::library, trLabel("Open the skill library")).c_str())) show_view(skills_view);
            cyui::space(2);
        }

        const auto groups = software_groups(cli_view);
        const auto findings = std::count_if(software_.findings.begin(), software_.findings.end(), [&](const SoftwareFinding& item) {
            return (item.kind == "cli") == cli_view;
        });
        const std::array stats{kit::Stat{"Groups", groups.size(), Tone::accent},
                               kit::Stat{"Results", static_cast<std::size_t>(findings), Tone::success},
                               kit::Stat{"Files inspected", software_.examined, Tone::info}};
        kit::stat_tiles("##software-stats", stats);
        const auto index = static_cast<std::size_t>(view_ - 1);
        draw_split([&] { draw_software_list(groups, index, cli_view); }, [&] { draw_software_details(groups, index); });
    }

    std::vector<SoftwareGroup> software_groups(bool cli_kind) const {
        std::vector<SoftwareGroup> groups;
        std::map<std::string, std::size_t> indices;
        for (const auto& item : software_.findings) {
            if ((item.kind == "cli") != cli_kind) continue;
            const auto name = cli_kind ? item.name : cymcp::path_utf8(item.path.filename());
            const auto key = group_key(name);
            const auto [index, inserted] = indices.try_emplace(key, groups.size());
            if (inserted) groups.push_back({key, name, {}});
            groups[index->second].variants.push_back(&item);
        }
        return groups;
    }

    void draw_software_list(const std::vector<SoftwareGroup>& groups, std::size_t index, bool cli_kind) {
        auto& search = software_search_[index];
        ImGui::Indent(dp(8));
        cyui::space(6);
        cyui::searchField("##software-search", search.data(), static_cast<int>(search.size()),
                          tr(cli_kind ? "Search CLIs" : "Search SDKs").c_str(), -dp(8));
        ImGui::Unindent(dp(8));
        cyui::space(4);
        const auto query = group_key(search.data());
        auto& selected = selected_software_group_[index];
        const auto match = [&](const SoftwareGroup& group) {
            if (contains_text(group.name, query)) return true;
            return std::any_of(group.variants.begin(), group.variants.end(), [&](const SoftwareFinding* item) {
                return contains_text(item->name, query) || contains_text(cymcp::path_utf8(item->path), query);
            });
        };
        if (std::none_of(groups.begin(), groups.end(), [&](const SoftwareGroup& group) { return group.key == selected && match(group); })) {
            const auto first = std::find_if(groups.begin(), groups.end(), match);
            selected = first == groups.end() ? std::string{} : first->key;
        }
        if (selected.empty()) {
            if (pending_software_.valid()) kit::empty_state(tr("Software scan in progress…"), {}, icon::search);
            else kit::empty_state(tr(cli_kind ? "No CLI detected" : "No SDK detected"), tr("Choose an application or a folder to look for CyMCPSDK markers and known CLIs."), icon::code);
            return;
        }
        if (kit::begin_rows("##rows")) {
            for (const auto& group : groups) {
                if (!match(group)) continue;
                const auto* first = group.variants.front();
                const auto subtitle = group.variants.size() == 1 ? tr(software_type(first->kind)) + "  ·  " + leaf_name(first->path.parent_path())
                                                                 : std::to_string(group.variants.size()) + " " + tr("Variants");
                if (kit::list_row(group.key.c_str(), group.name, subtitle, cyui::toneColour(cli_kind ? Tone::info : Tone::success),
                                  group.variants.size() > 1 ? std::to_string(group.variants.size()) : std::string{},
                                  selected == group.key)) selected = group.key;
            }
        }
        kit::end_rows();
    }

    void draw_software_details(const std::vector<SoftwareGroup>& groups, std::size_t index) {
        const auto& active = selected_software_group_[index];
        const auto group = std::find_if(groups.begin(), groups.end(), [&](const SoftwareGroup& item) { return item.key == active; });
        if (group == groups.end()) {
            if (pending_software_.valid()) kit::empty_state(tr("Software scan in progress…"), {}, icon::search);
            else kit::empty_state(tr("Select a result"), tr("See its path and detection evidence."), icon::code);
            return;
        }
        const cyui::ScopedID id(group->key.c_str());
        current_context_["selection"] = group->name;
        auto& selected_id = selected_software_variants_[std::to_string(index) + group->key];
        const auto key = [](const SoftwareFinding& item) { return cymcp::path_utf8(item.path) + '#' + item.kind; };
        const auto selected_it = std::find_if(group->variants.begin(), group->variants.end(), [&](const auto* item) {
            return key(*item) == selected_id;
        });
        const auto* selected = selected_it == group->variants.end() ? group->variants.front() : *selected_it;

        cyui::heading(group->name.c_str(), 2);
        cyui::badge(tr(software_type(selected->kind)).c_str(), Tone::info);
        if (group->variants.size() > 1) {
            cyui::space(6);
            cyui::overline((tr("VARIANTS") + "  " + std::to_string(group->variants.size())).c_str(), cyui::palette().textFaint);
            Chips chips;
            std::map<std::string, int> names;
            for (const auto* item : group->variants) {
                auto label = item->kind == "cli" ? cymcp::path_utf8(item->path.filename()) : tr(software_type(item->kind));
                const auto duplicate = ++names[label];
                if (duplicate > 1) label += " #" + std::to_string(duplicate);
                if (chips.draw(label, key(*item), item == selected)) selected = item;
                cyui::tooltip(cymcp::path_utf8(item->path).c_str());
            }
        }
        selected_id = key(*selected);
        cyui::space(8);
        const auto path = cymcp::path_utf8(selected->path);
        if (cyui::beginProperties("##software-properties", 112)) {
            cyui::property(tr("Type").c_str());
            kit::wrapped(tr(software_type(selected->kind)));
            cyui::property(tr("Path").c_str());
            kit::wrapped_dim(path);
            cyui::property(tr("Evidence").c_str());
            for (const auto& item : selected->evidence) kit::wrapped("•  " + tr(item.c_str()));
            cyui::endProperties();
        }
        cyui::space(6);
        if (cyui::button(kit::with_icon(icon::copy, trLabel("Copy path")).c_str())) kit::copy_to_clipboard(path);
    }

    // ---------------------------------------------------------------- Skills

    void draw_skills_page() {
        const auto& found = software_.skills;
        const auto collect = found.empty() ? std::string{} :
            trLabel("Collect detected skills") + "  (" + std::to_string(found.size()) + ")###collect-found";
        std::vector<kit::Action> actions{{icon::refresh, trLabel("Refresh"), cyui::Variant::secondary, !skills_.busy(), {}}};
        if (!found.empty()) actions.push_back({icon::importFile, collect, cyui::Variant::primary, !skills_.busy(), {}});
        switch (kit::page_header(tr("Skills").c_str(), tr("Skills collected from connectors and projects."), actions)) {
        case 0: skills_.refresh(); break;
        case 1: skills_.collect_found(found); break;
        default: break;
        }
        current_context_["selection"] = skills_.draw_library();
    }

    // ---------------------------------------------------------------- Journal

    void draw_journal_page() {
        const auto entries = journal_->entries();
        const std::array actions{
            kit::Action{icon::copy, trLabel("Copy journal"), cyui::Variant::secondary, !entries.empty(), {}},
            kit::Action{icon::trash, trLabel("Clear"), cyui::Variant::ghost, !entries.empty(), {}}};
        switch (kit::page_header(tr("Journal").c_str(), tr("Inventory scans, connector calls and assistant activity."), actions)) {
        case 0: kit::copy_to_clipboard(journal_->json_lines()); break;
        case 1: journal_->clear(); break;
        default: break;
        }
        cyui::searchField("##journal-search", journal_search_.data(), static_cast<int>(journal_search_.size()),
                          tr("Search the journal").c_str(), dp(300));
        ImGui::SameLine();
        static constexpr const char* filters[] = {"All", "Problems"};
        cyui::segmented("##journal-filter", filters, 2, &journal_filter_);
        cyui::space(4);

        const auto query = group_key(journal_search_.data());
        std::vector<const JournalEntry*> rows;
        for (auto item = entries.rbegin(); item != entries.rend(); ++item) {
            const auto tone = journal_tone(item->status);
            if (journal_filter_ == 1 && tone != Tone::error && tone != Tone::warning) continue;
            if (!query.empty() && !contains_text(item->source, query) && !contains_text(item->operation, query) &&
                !contains_text(tr(item->operation.c_str()), query) && !contains_text(item->detail, query) &&
                !contains_text(item->status, query)) continue;
            rows.push_back(&*item);
        }
        if (kit::begin_pane("##journal-pane", ImVec2(0, 0), false)) {
            if (entries.empty()) kit::empty_state(tr("The journal is empty"), tr("Inventory scans and connector activity appear here."), icon::list);
            else if (rows.empty()) kit::empty_state(tr("No matching entries"), {}, icon::search);
            else draw_journal_table(rows);
        }
        kit::end_pane();
    }

    static void draw_journal_table(const std::vector<const JournalEntry*>& rows) {
        const cyui::ScopedColor ground(ImGuiCol_TableRowBgAlt, cyui::alpha(cyui::palette().bg3, 0.35f));
        if (!ImGui::BeginTable("##journal", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable |
                                                ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX)) return;
        ImGui::TableSetupColumn(tr("Time").c_str(), ImGuiTableColumnFlags_WidthFixed, dp(76));
        ImGui::TableSetupColumn(tr("Status").c_str(), ImGuiTableColumnFlags_WidthFixed, dp(110));
        ImGui::TableSetupColumn(tr("Source").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.9f);
        ImGui::TableSetupColumn(tr("Operation").c_str(), ImGuiTableColumnFlags_WidthStretch, 1.1f);
        ImGui::TableSetupColumn("ms", ImGuiTableColumnFlags_WidthFixed, dp(52));
        ImGui::TableSetupColumn(tr("Details").c_str(), ImGuiTableColumnFlags_WidthStretch, 2.2f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(rows.size()));
        while (clipper.Step()) for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const auto& item = *rows[static_cast<std::size_t>(row)];
            const cyui::ScopedID id(row);
            const auto time = local_time(static_cast<std::time_t>(item.timestamp_ms / 1000));
            const auto operation = tr(item.operation.c_str());
            const auto status = tr(item.status.c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); cyui::textFaint("%s", time.c_str());
            ImGui::TableNextColumn();
            kit::dot(cyui::toneColour(journal_tone(item.status)));
            ImGui::SameLine(0, dp(2));
            ImGui::TextUnformatted(status.c_str());
            ImGui::TableNextColumn(); ImGui::TextUnformatted(item.source.c_str()); cyui::tooltip(item.source.c_str());
            ImGui::TableNextColumn(); ImGui::TextUnformatted(operation.c_str()); cyui::tooltip(operation.c_str());
            ImGui::TableNextColumn();
            if (item.elapsed_ms > 0) cyui::textDim("%.0f", item.elapsed_ms);
            ImGui::TableNextColumn(); cyui::textDim("%s", item.detail.c_str()); cyui::tooltip(item.detail.c_str());
        }
        ImGui::EndTable();
    }

    // ---------------------------------------------------------------- Settings

    bool apply_settings(const AppSettings& settings) {
        try { save_app_settings(settings); }
        catch (const std::exception& error) {
            settings_error_ = tr(cymcp::redact_error_message(error.what()).c_str());
            return false;
        }
        const bool port_scan_changed = options_.scan_ports != settings.scan_ports;
        if (settings_draft_.scan_ports == options_.scan_ports) settings_draft_.scan_ports = settings.scan_ports;
        settings_ = settings;
        options_.scan_ports = settings.scan_ports;
        resident_.sync_options(options_, settings_.service);
        if (port_scan_changed) refresh();
        settings_error_.clear();
        return true;
    }

    void reset_settings_draft() {
        settings_draft_ = settings_;
        settings_draft_.scan_ports = options_.scan_ports;
        try { login_saved_ = autostart_enabled(); }
        catch (const std::exception&) { settings_error_ = tr("Could not change automatic startup."); }
        login_draft_ = login_saved_;
    }

    bool settings_dirty() const {
        auto saved = settings_;
        saved.scan_ports = options_.scan_ports;
        return settings_draft_ != saved || login_draft_ != login_saved_;
    }

    void save_settings_draft() {
        if (!apply_settings(settings_draft_)) return;
        try {
            if (login_draft_ != autostart_enabled()) {
                auto login_options = options_;
                login_options.scan_ports = true;
                set_autostart(login_draft_, login_options);
                tray_login_changed = true;
            }
            login_saved_ = login_draft_;
            cyui::notify(tr("Settings saved").c_str(), Tone::success);
        } catch (const std::exception&) {
            settings_error_ = tr("Could not change automatic startup.");
        }
    }

    void draw_settings_page() {
        const bool dirty = settings_dirty();
        const std::array actions{
            kit::Action{nullptr, trLabel("Discard"), cyui::Variant::ghost, dirty, {}},
            kit::Action{icon::save, trLabel("Save"), cyui::Variant::primary,
                        (dirty || !settings_error_.empty()) && !(settings_draft_.scan_ports != options_.scan_ports && pending_.valid()), {}}};
        const auto subtitle = dirty ? tr("Unsaved changes") : tr("Application, inventory sources and MCP service.");
        switch (kit::page_header(tr("Settings").c_str(), subtitle, actions)) {
        case 0:
            settings_error_.clear();
            reset_settings_draft();
            break;
        case 1: save_settings_draft(); break;
        default: break;
        }

        const cyui::ScopedColor ground(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
        if (ImGui::BeginChild("##settings-body", ImVec2(0, 0))) {
            const auto available = ImGui::GetContentRegionAvail().x;
            const auto column = std::min(available, dp(780));
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (available - column) * 0.5f));
            if (ImGui::BeginChild("##settings-column", ImVec2(column, 0), ImGuiChildFlags_AutoResizeY)) {
                if (!settings_error_.empty()) cyui::callout(Tone::error, settings_error_.c_str());
                if (cyui::beginCard("##general-settings", tr("General").c_str())) draw_app_settings_fields();
                cyui::endCard();
                cyui::space(8);
                if (cyui::beginCard("##inventory-settings", tr("Inventory sources").c_str())) draw_inventory_sources();
                cyui::endCard();
                cyui::space(8);
                if (cyui::beginCard("##service-settings", tr("MCP service (HTTP)").c_str())) draw_service_settings_fields();
                cyui::endCard();
                cyui::space(8);
                if (cyui::beginCard("##client-settings", tr("Client configuration").c_str())) {
                    kit::wrapped_dim(tr("Add this entry to the MCP configuration of your AI client."));
                    cyui::space(2);
                    draw_transport_choice("##settings-transport");
                }
                cyui::endCard();
                cyui::space(12);
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();

        current_context_["selection"] = {{"mcp_service_running", resident_.running()},
                                         {"mcp_port", settings_.service.port},
                                         {"mcp_requires_token", settings_.service.require_token},
                                         {"mcp_start_on_launch", settings_.service.start_on_launch},
                                         {"refresh_interval_seconds", settings_.refresh_seconds}};
    }

    void draw_app_settings_fields() {
        if (!cyui::beginProperties("##general-fields", 210)) return;
#ifdef _WIN32
        cyui::property(tr("Start with Windows").c_str());
#else
        cyui::property(tr("Start at login").c_str());
#endif
        cyui::toggleSwitch("##login", &login_draft_);
        cyui::property(tr("Local TCP ports").c_str(), tr("Look for MCP servers listening on local TCP ports.").c_str());
        cyui::toggleSwitch("##scan-ports", &settings_draft_.scan_ports);
        cyui::property(tr("Automatic refresh").c_str(), tr("How often the MCP inventory runs again in the background.").c_str());
        constexpr const char* intervals[] = {"Off", "15 seconds", "30 seconds", "1 minute", "2 minutes", "5 minutes"};
        static_assert(std::size(intervals) == AppSettings::refresh_intervals.size());
        const auto found = std::find(AppSettings::refresh_intervals.begin(), AppSettings::refresh_intervals.end(),
                                     settings_draft_.refresh_seconds);
        const auto selected = found == AppSettings::refresh_intervals.end() ? std::size_t{0} :
                              static_cast<std::size_t>(found - AppSettings::refresh_intervals.begin());
        ImGui::SetNextItemWidth(std::min(ImGui::GetContentRegionAvail().x, dp(220)));
        if (ImGui::BeginCombo("##refresh-interval", tr(intervals[selected]).c_str())) {
            for (std::size_t i = 0; i < std::size(intervals); ++i) {
                if (ImGui::Selectable(tr(intervals[i]).c_str(), i == selected))
                    settings_draft_.refresh_seconds = AppSettings::refresh_intervals[i];
            }
            ImGui::EndCombo();
        }
        cyui::endProperties();
    }

    void draw_inventory_sources() {
        const cyui::ScopedDisabled scanning(pending_.valid() || pending_path_.valid());
        if (!cyui::beginProperties("##source-fields", 210)) return;
        cyui::property(tr("Project folder").c_str());
        kit::wrapped(cymcp::path_utf8(options_.workspace));
        if (cyui::button(kit::with_icon(icon::folder, trLabel("Change…")).c_str())) pick_path(Pick::workspace);
        cyui::property(tr("Extra configuration").c_str());
        const bool has_config = !options_.extra_config.empty();
        if (has_config) kit::wrapped(cymcp::path_utf8(options_.extra_config));
        else kit::wrapped_dim(tr("No extra configuration"));
        if (cyui::button(kit::with_icon(icon::openFile, trLabel(has_config ? "Change…" : "Add…")).c_str()))
            pick_path(Pick::configuration);
        if (has_config) {
            ImGui::SameLine();
            if (cyui::ghostButton(trLabel("Remove").c_str())) {
                options_.extra_config.clear();
                refresh();
            }
        }
        cyui::endProperties();
    }

    void draw_service_status(bool controls_enabled = true) {
        const bool running = resident_.running();
        kit::dot(cyui::toneColour(running ? Tone::success : Tone::neutral), ImGui::GetFrameHeight());
        ImGui::SameLine(0, dp(2));
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(tr(running ? "MCP service running" : "MCP service stopped").c_str());
        if (running) {
            ImGui::SameLine();
            cyui::textDim("%s", mcp_service_url(settings_.service.port).c_str());
        }
        const auto label = kit::with_icon(icon::power, trLabel(running ? "Stop MCP service" : "Start MCP service"));
        ImGui::SameLine();
        cyui::alignRight(cyui::buttonWidth(label.c_str()));
        {
            const cyui::ScopedDisabled disabled(!controls_enabled);
            if (cyui::button(label.c_str(), running ? cyui::Variant::secondary : cyui::Variant::success))
                resident_.set_running(!running, options_, settings_.service);
        }
        if (!controls_enabled) cyui::tooltip(tr("Save the settings before starting or stopping the service.").c_str());
        if (!resident_.error().empty()) cyui::callout(Tone::error, resident_.error().c_str());
    }

    void draw_service_settings_fields() {
        auto& service = settings_draft_.service;
        draw_service_status(service.port == settings_.service.port && service.require_token == settings_.service.require_token);
        cyui::space(6);
        if (!cyui::beginProperties("##service-fields", 210)) return;
        cyui::property(tr("Start with the app").c_str());
        cyui::toggleSwitch("##service-launch", &service.start_on_launch);
        cyui::property(tr("Service port").c_str());
        ImGui::SetNextItemWidth(std::min(ImGui::GetContentRegionAvail().x, dp(140)));
        ImGui::InputInt("##service-port", &service.port, 0);
        cyui::property(tr("Access mode").c_str());
        ImGui::SetNextItemWidth(std::min(ImGui::GetContentRegionAvail().x, dp(220)));
        if (ImGui::BeginCombo("##service-access", tr(service.require_token ? "Token required" : "Free (local)").c_str())) {
            if (ImGui::Selectable(tr("Token required").c_str(), service.require_token)) service.require_token = true;
            if (ImGui::Selectable(tr("Free (local)").c_str(), !service.require_token)) service.require_token = false;
            ImGui::EndCombo();
        }
        if (!service.require_token) cyui::textColoured(cyui::toneColour(Tone::warning), "%s",
                                                       tr("Any local client can use the tools without a token.").c_str());
        cyui::endProperties();
    }

    // ---------------------------------------------------------------- AI client setup

    void draw_transport_choice(const char* id) {
        static constexpr const char* transports[] = {"stdio", "MCP service (HTTP)"};
        cyui::segmented(id, transports, 2, &setup_transport_);
        cyui::space(4);
        const bool http = setup_transport_ == 1;
        if (http) {
            if (!resident_.running()) {
                cyui::callout(Tone::info, tr("Start the MCP service to copy the HTTP configuration.").c_str());
                const auto start = kit::with_icon(icon::power, trLabel("Start MCP service"));
                if (cyui::button(start.c_str(), cyui::Variant::success)) resident_.set_running(true, options_, settings_.service);
                if (!resident_.error().empty()) cyui::callout(Tone::error, resident_.error().c_str());
                return;
            }
        } else {
            kit::wrapped_dim(tr("The AI can list servers, read connection details, edit annotations and scan for SDKs and CLIs."));
        }
        draw_client_config(http);
    }

    void draw_setup() {
        const auto title = tr("AI setup") + "###setup";
        if (!cyui::beginModal(title.c_str(), &setup_open_, 640)) return;
        kit::wrapped_dim(tr("Add this entry to the MCP configuration of your AI client."));
        cyui::space(4);
        draw_transport_choice("##mcp-transport");
        cyui::endModal();
    }

    void draw_client_config(bool http) {
        auto args = Json::array({"--workspace", cymcp::path_utf8(options_.workspace)});
        if (!options_.extra_config.empty()) {
            args.push_back("--config");
            args.push_back(cymcp::path_utf8(options_.extra_config));
        }
        if (!options_.scan_ports) args.push_back("--no-port-scan");
        auto config = http ? resident_.configuration() :
            Json({{"mcpServers", {{"cymcphub", {{"command", app_mcp_path()}, {"args", args}}}}}});
        auto displayed = config;
        const bool token = http && config["mcpServers"]["cymcphub"].contains("headers");
        if (token) displayed["mcpServers"]["cymcphub"]["headers"]["Authorization"] = "Bearer <" + tr("Token hidden") + ">";
        kit::code_box("##setup-json", displayed.dump(2), 150);
        if (cyui::primaryButton(kit::with_icon(icon::copy, trLabel("Copy configuration")).c_str())) kit::copy_to_clipboard(config.dump(2));
        if (token) {
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            cyui::textColoured(cyui::toneColour(Tone::warning), "%s", tr("The copied configuration includes the token.").c_str());
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
    register_update_translations();
    Window app(window, std::move(options));
    const Draw tick = [&] { app.tick(); };
    const Draw main = [&] { app.draw(); };
    const Draw assistant = [&] { app.draw_assistant_window(); };
    while (frame(tick, main, assistant, app.assistant_open(), app.assistant_focus())) {}
}
} // namespace hub
