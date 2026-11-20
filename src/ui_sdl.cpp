// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "ui.hpp"
#include "autostart.hpp"
#include "platform.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cyui/Core.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <iostream>

namespace {
using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;

struct Sdl {
    Sdl() {
        SDL_SetAppMetadata("CyMCPHub", nullptr, "com.cyberalien.cymcphub");
        SDL_SetHint(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, "0");
        if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
    }
    ~Sdl() { SDL_Quit(); }
};

struct Screen {
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window{nullptr, SDL_DestroyWindow};
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer{nullptr, SDL_DestroyRenderer};
    ImGuiContext* context = nullptr;
    float dpi = 1;

    Screen(const char* title, int width, int height, int min_width, int min_height, SDL_Surface* icon) {
        window.reset(SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN));
        if (!window) throw std::runtime_error(SDL_GetError());
        if (icon) SDL_SetWindowIcon(window.get(), icon);
        SDL_SetWindowMinimumSize(window.get(), min_width, min_height);
        renderer.reset(SDL_CreateRenderer(window.get(), nullptr));
        if (!renderer) throw std::runtime_error(SDL_GetError());
        SDL_SetRenderVSync(renderer.get(), 1);
        context = ImGui::CreateContext();
        ImGui::SetCurrentContext(context);
        ImGui::GetIO().IniFilename = nullptr;
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        cyui::Config config;
        config.themeKey = "amber-night";
        config.density = cyui::Density::comfortable;
        const float scale = SDL_GetWindowDisplayScale(window.get());
        const float pixels = SDL_GetWindowPixelDensity(window.get());
        dpi = scale > 0 && pixels > 0 ? scale / pixels : 1.f;
        config.dpiScale = dpi;
        cyui::apply(config);
        cyui::setLocale(hub::french_locale() ? "fr" : "en");
        cyui::loadFonts();
        ImGui_ImplSDL3_InitForSDLRenderer(window.get(), renderer.get());
        ImGui_ImplSDLRenderer3_Init(renderer.get());
    }

    ~Screen() {
        ImGui::SetCurrentContext(context);
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext(context);
    }

    SDL_WindowID id() const { return SDL_GetWindowID(window.get()); }
    bool visible() const { return !(SDL_GetWindowFlags(window.get()) & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED)); }
    void event(const SDL_Event& event) {
        ImGui::SetCurrentContext(context);
        ImGui_ImplSDL3_ProcessEvent(&event);
    }

    void render(const hub::Draw& draw) {
        ImGui::SetCurrentContext(context);
        const float scale = SDL_GetWindowDisplayScale(window.get());
        const float pixels = SDL_GetWindowPixelDensity(window.get());
        const float current_dpi = scale > 0 && pixels > 0 ? scale / pixels : 1.f;
        if (current_dpi != dpi) { dpi = current_dpi; cyui::applyDpi(dpi); }
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        draw();
        cyui::renderNotifications();
        ImGui::Render();
        const auto* data = ImGui::GetDrawData();
        SDL_SetRenderScale(renderer.get(), data->FramebufferScale.x, data->FramebufferScale.y);
        const auto color = ImGui::ColorConvertU32ToFloat4(cyui::palette().bg0);
        SDL_SetRenderDrawColorFloat(renderer.get(), color.x, color.y, color.z, color.w);
        SDL_RenderClear(renderer.get());
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer.get());
        SDL_RenderPresent(renderer.get());
    }
};

struct Tray {
    std::unique_ptr<SDL_Tray, decltype(&SDL_DestroyTray)> icon{nullptr, SDL_DestroyTray};
    SDL_Window* window;
    const hub::Options& options;

    Tray(SDL_Window* host, const hub::Options& startup_options, SDL_Surface* logo) : window(host), options(startup_options) {
        const Surface surface(logo ? nullptr : SDL_CreateSurface(24, 24, SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
        if (surface) {
            SDL_FillSurfaceRect(surface.get(), nullptr, SDL_MapSurfaceRGBA(surface.get(), 0, 0, 0, 0));
            const SDL_Rect outer{3, 3, 18, 18}, inner{7, 7, 10, 10};
            SDL_FillSurfaceRect(surface.get(), &outer, SDL_MapSurfaceRGBA(surface.get(), 240, 166, 63, 255));
            SDL_FillSurfaceRect(surface.get(), &inner, SDL_MapSurfaceRGBA(surface.get(), 38, 42, 52, 255));
        }
        icon.reset(SDL_CreateTray(logo ? logo : surface.get(), "CyMCPHub"));
        if (!icon) return;
        const auto menu = SDL_CreateTrayMenu(icon.get());
        if (!menu) { icon.reset(); return; }
        const auto fr = hub::french_locale();
        auto insert = [&](const char* label, SDL_TrayEntryFlags flags, SDL_TrayCallback callback) {
            if (auto* item = SDL_InsertTrayEntryAt(menu, -1, label, flags)) SDL_SetTrayEntryCallback(item, callback, this);
        };
        insert(fr ? "Ouvrir CyMCPHub" : "Open CyMCPHub", SDL_TRAYENTRY_BUTTON, [](void* data, SDL_TrayEntry*) {
            auto* self = static_cast<Tray*>(data);
            SDL_ShowWindow(self->window);
            SDL_RaiseWindow(self->window);
        });
        insert(fr ? "Ouvrir l’assistant" : "Open Assistant", SDL_TRAYENTRY_BUTTON, [](void*, SDL_TrayEntry*) {
            hub::tray_assistant_requested = true;
            SDL_Event wake{};
            wake.type = SDL_EVENT_USER;
            SDL_PushEvent(&wake);
        });
        bool login = false;
        try { login = hub::autostart_enabled(); } catch (const std::exception&) {}
        insert(fr ? "Démarrer à l’ouverture de session" : "Start at login",
               SDL_TRAYENTRY_CHECKBOX | (login ? SDL_TRAYENTRY_CHECKED : 0), [](void* data, SDL_TrayEntry* item) {
            auto* self = static_cast<Tray*>(data);
            const auto checked = SDL_GetTrayEntryChecked(item);
            try { hub::set_autostart(checked, self->options); }
            catch (const std::exception&) {
                SDL_SetTrayEntryChecked(item, !checked);
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "CyMCPHub",
                    hub::french_locale() ? "Impossible de modifier le démarrage automatique." : "Could not change automatic startup.",
                    self->window);
            }
        });
        insert(fr ? "Quitter CyMCPHub" : "Quit CyMCPHub", SDL_TRAYENTRY_BUTTON, [](void*, SDL_TrayEntry*) {
            SDL_Event event{};
            event.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&event);
        });
    }
};

void run(hub::Arguments args) {
    Sdl sdl;
    hub::fs::create_directories(hub::data_path());
    const hub::FileLock instance(hub::data_path() / "desktop.lock");
    const auto* base = SDL_GetBasePath();
    const Surface logo(base ? SDL_LoadPNG((std::string(base) + "cymcphub-logo.png").c_str()) : nullptr, SDL_DestroySurface);
    Screen main("CyMCPHub", 1200, 790, 860, 610, logo.get());
    std::unique_ptr<Screen> assistant;
    const Tray tray(main.window.get(), args.options, logo.get());
    if (!args.tray || !tray.icon) SDL_ShowWindow(main.window.get());

    hub::draw_hub(main.window.get(), args.options, [&](const hub::Draw& draw_main, const hub::Draw& draw_assistant,
                                                     bool& assistant_open, bool& focus) {
        SDL_Event event{};
        const auto wait = main.visible() || (assistant && assistant_open) ? 0 : 200;
        if (SDL_WaitEventTimeout(&event, wait)) do {
            if (event.type == SDL_EVENT_QUIT) return false;
            auto* affected = SDL_GetWindowFromEvent(&event);
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                if (event.window.windowID == main.id()) {
                    if (!tray.icon) return false;
                    SDL_HideWindow(main.window.get());
                } else if (assistant && event.window.windowID == assistant->id()) {
                    assistant_open = false;
                    SDL_HideWindow(assistant->window.get());
                }
            }
            if (!affected || affected == main.window.get()) main.event(event);
            if (assistant && (!affected || affected == assistant->window.get())) assistant->event(event);
        } while (SDL_PollEvent(&event));

        // Keep the screen context current for the shared catalogue and the assistant controller.
        if (main.visible() || hub::tray_assistant_requested || assistant_open) main.render(draw_main);
        if (assistant_open) {
            if (!assistant) assistant = std::make_unique<Screen>("Assistant · CyMCPHub", 700, 620, 465, 380, logo.get());
            if (focus) {
                SDL_ShowWindow(assistant->window.get());
                SDL_RaiseWindow(assistant->window.get());
                focus = false;
            }
            if (assistant->visible()) assistant->render(draw_assistant);
        }
        ImGui::SetCurrentContext(main.context);
        SDL_Delay(8);
        return true;
    });
}
} // namespace

int main(int argc, char** argv) {
    try {
        auto arguments = hub::parse_arguments({argv, argv + argc});
        if (arguments.help || arguments.list) {
            std::cout << "CyMCPHub [--tray] [--workspace PATH] [--config FILE] [--no-port-scan]\n"
                         "CyMCPHubMcp --list exports the inventory.\n";
        } else run(std::move(arguments));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << cymcp::redact_error_message(error.what()) << '\n';
        return 1;
    }
}
