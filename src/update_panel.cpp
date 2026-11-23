// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "update_panel.hpp"
#include "platform.hpp"
#include "ui_kit.hpp"

#include <cyui/Core.h>
#ifdef _WIN32
#include <shellapi.h>
#else
#include <SDL3/SDL.h>
#endif

namespace hub {
namespace {
using cyui::tr;
using cyui::trLabel;
namespace icon = cyui::icon;

bool open_release(WindowHandle window, const std::string& url) {
#ifdef _WIN32
    const auto address = from_utf8(url).wstring();
    return reinterpret_cast<INT_PTR>(ShellExecuteW(window, L"open", address.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
#else
    (void)window;
    return SDL_OpenURL(url.c_str());
#endif
}

template <class T>
bool ready(std::future<T>& future) {
    return future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}
} // namespace

void register_update_translations() {
    cyui::registerTranslations("fr", {
        {"Current version", "Version actuelle"}, {"Check for updates", "Rechercher une mise à jour"},
        {"Checking GitHub…", "Vérification sur GitHub…"}, {"Downloading and verifying…", "Téléchargement et vérification…"},
        {"CyMCPHub is up to date.", "CyMCPHub est à jour."}, {"Available version", "Version disponible"},
        {"A new version is available.", "Une nouvelle version est disponible."},
        {"View release", "Voir la release"}, {"Could not open GitHub.", "Impossible d’ouvrir GitHub."},
        {"Download installer", "Télécharger l’installateur"}, {"Quit and install", "Quitter et installer"},
        {"Could not launch the installer.", "Impossible de lancer l’installateur."},
        {"This release has no compatible installer.", "Installateur compatible indisponible pour cette version."},
        {"The installer is verified with its SHA-256 checksum before it runs.",
         "L’installateur est vérifié avec son empreinte SHA-256 avant son lancement."},
        {"Close", "Fermer"}
    });
}

UpdatePanel::~UpdatePanel() { cancellation_.cancel(); }

void UpdatePanel::check() {
    if (checking_.valid() || downloading_.valid()) return;
    error_.clear();
    checked_ = false;
    update_.reset();
    downloaded_.clear();
    cancellation_ = {};
    checking_ = std::async(std::launch::async, [cancel = cancellation_] { return check_update(cancel); });
}

void UpdatePanel::draw(WindowHandle window, bool& open) {
    try {
        if (ready(checking_)) {
            checked_ = true;
            update_ = checking_.get();
        }
        if (ready(downloading_)) downloaded_ = downloading_.get();
    } catch (const std::exception& error) {
        error_ = cymcp::redact_error_message(error.what());
    }
    if (open && !was_open_ && !checked_) check();
    was_open_ = open;

    const auto title = tr("Updates") + "###updates";
    if (!cyui::beginModal(title.c_str(), &open, 480)) return;
    const bool busy = checking_.valid() || downloading_.valid();
    if (cyui::beginProperties("##update-versions", 150)) {
        cyui::property(tr("Current version").c_str());
        ImGui::TextUnformatted(CYMCP_HUB_VERSION);
        if (update_) {
            cyui::property(tr("Available version").c_str());
            cyui::textColoured(cyui::toneColour(cyui::Tone::success), "%s", update_->version.c_str());
        }
        cyui::endProperties();
    }
    cyui::space(4);
    if (checking_.valid() || downloading_.valid()) {
        cyui::spinner("##update-busy", ImGui::GetFontSize() * 0.36f);
        ImGui::SameLine();
        cyui::textDim("%s", tr(checking_.valid() ? "Checking GitHub…" : "Downloading and verifying…").c_str());
    } else if (checked_ && !update_ && error_.empty()) {
        cyui::callout(cyui::Tone::success, tr("CyMCPHub is up to date.").c_str());
    } else if (update_) {
        cyui::callout(cyui::Tone::info, tr("A new version is available.").c_str());
    }
    if (!error_.empty()) cyui::callout(cyui::Tone::error, error_.c_str());
#ifdef _WIN32
    if (update_ && update_->installer) kit::wrapped_dim(tr("The installer is verified with its SHA-256 checksum before it runs."));
    else if (update_) kit::wrapped_dim(tr("This release has no compatible installer."));
#endif
    cyui::space(8);

    {
        const cyui::ScopedDisabled disabled(busy);
        if (cyui::button(kit::with_icon(icon::refresh, trLabel("Check for updates")).c_str())) check();
    }
    if (update_) {
        ImGui::SameLine();
        if (cyui::button(kit::with_icon(icon::globe, trLabel("View release")).c_str()) && !open_release(window, update_->release_url))
            error_ = tr("Could not open GitHub.");
#ifdef _WIN32
        if (update_->installer) {
            ImGui::SameLine();
            const cyui::ScopedDisabled disabled(busy);
            if (downloaded_.empty()) {
                if (cyui::primaryButton(kit::with_icon(icon::download, trLabel("Download installer")).c_str())) {
                    error_.clear();
                    cancellation_ = {};
                    downloading_ = std::async(std::launch::async, [installer = *update_->installer, cancel = cancellation_] {
                        return download_installer(installer, cancel);
                    });
                }
            } else if (cyui::primaryButton(kit::with_icon(icon::power, trLabel("Quit and install")).c_str())) {
                error_.clear();
                try {
                    verify_installer(downloaded_, *update_->installer);
                    if (reinterpret_cast<INT_PTR>(ShellExecuteW(window, L"open", downloaded_.c_str(),
                            nullptr, nullptr, SW_SHOWNORMAL)) <= 32)
                        throw std::runtime_error(tr("Could not launch the installer."));
                    PostQuitMessage(0);
                } catch (const std::exception& error) {
                    error_ = cymcp::redact_error_message(error.what());
                    downloaded_.clear();
                }
            }
        }
#endif
    }
    cyui::endModal();
}
} // namespace hub
