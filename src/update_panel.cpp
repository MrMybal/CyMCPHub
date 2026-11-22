// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "update_panel.hpp"
#include "platform.hpp"

#include <cyui/Core.h>
#ifdef _WIN32
#include <shellapi.h>
#else
#include <SDL3/SDL.h>
#endif

namespace hub {
namespace {
bool open_release(WindowHandle window, const std::string& url) {
#ifdef _WIN32
    const std::wstring address(url.begin(), url.end());
    return reinterpret_cast<INT_PTR>(ShellExecuteW(window, L"open", address.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
#else
    (void)window;
    return SDL_OpenURL(url.c_str());
#endif
}
} // namespace

UpdatePanel::~UpdatePanel() { cancellation_.cancel(); }

void UpdatePanel::draw(WindowHandle window) {
    const auto fr = french_locale();
    const auto ready = [](auto& future) {
        return future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    };
    try {
        if (ready(checking_)) {
            update_ = checking_.get();
            checked_ = true;
        }
        if (ready(downloading_)) downloaded_ = downloading_.get();
    } catch (const std::exception& error) {
        error_ = cymcp::redact_error_message(error.what());
    }

    const auto title = fr ? "Mises à jour###updates" : "Updates###updates";
    ImGui::SetNextWindowSize(cyui::dp(460, 0), ImGuiCond_Appearing);
    bool open = true;
    if (!ImGui::BeginPopupModal(title, &open, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::Text("%s %s", fr ? "Version actuelle" : "Current version", CYMCP_HUB_VERSION);
    const bool busy = checking_.valid() || downloading_.valid();
    {
        const cyui::ScopedDisabled disabled(busy);
        if (cyui::button(fr ? "Rechercher une mise à jour" : "Check for updates")) {
            error_.clear();
            checked_ = false;
            update_.reset();
            downloaded_.clear();
            cancellation_ = {};
            checking_ = std::async(std::launch::async, [cancel = cancellation_] { return check_update(cancel); });
        }
    }
    if (checking_.valid()) ImGui::TextUnformatted(fr ? "Vérification sur GitHub…" : "Checking GitHub…");
    if (downloading_.valid()) ImGui::TextUnformatted(fr ? "Téléchargement et vérification…" : "Downloading and verifying…");
    if (checked_ && !update_) ImGui::TextUnformatted(fr ? "Aucune version plus récente." : "No newer version is available.");

    if (update_) {
        ImGui::Text("%s %s", fr ? "Version disponible" : "Available version", update_->version.c_str());
        if (cyui::button(fr ? "Voir la release" : "View release") && !open_release(window, update_->release_url))
            error_ = fr ? "Impossible d’ouvrir GitHub." : "Could not open GitHub.";
#ifdef _WIN32
        if (update_->installer) {
            const cyui::ScopedDisabled disabled(busy);
            if (downloaded_.empty()) {
                if (cyui::primaryButton(fr ? "Télécharger l’installateur" : "Download installer")) {
                    error_.clear();
                    cancellation_ = {};
                    downloading_ = std::async(std::launch::async, [installer = *update_->installer, cancel = cancellation_] {
                        return download_installer(installer, cancel);
                    });
                }
            } else if (cyui::primaryButton(fr ? "Quitter et installer" : "Quit and install")) {
                error_.clear();
                try {
                    verify_installer(downloaded_, *update_->installer);
                    if (reinterpret_cast<INT_PTR>(ShellExecuteW(window, L"open", downloaded_.c_str(),
                            nullptr, nullptr, SW_SHOWNORMAL)) <= 32)
                        throw std::runtime_error(fr ? "Impossible de lancer l’installateur." : "Could not launch the installer.");
                    PostQuitMessage(0);
                } catch (const std::exception& error) {
                    error_ = cymcp::redact_error_message(error.what());
                    downloaded_.clear();
                }
            }
        } else {
            ImGui::TextWrapped("%s", fr ? "Installateur compatible indisponible pour cette version."
                                        : "This release has no compatible installer.");
        }
#endif
    }
    if (!error_.empty()) cyui::callout(cyui::Tone::error, error_.c_str());
    ImGui::EndPopup();
}
} // namespace hub
