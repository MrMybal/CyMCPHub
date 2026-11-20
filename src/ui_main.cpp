// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "ui.hpp"

#include <cyui/Core.h>
#include <d3d11.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <shellapi.h>
#include <wrl/client.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {
using Microsoft::WRL::ComPtr;

struct Renderer {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swap_chain;
    ComPtr<ID3D11RenderTargetView> target;

    bool create(HWND window) {
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferCount = 2;
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.OutputWindow = window;
        desc.SampleDesc.Count = 1;
        desc.Windowed = TRUE;
        auto result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                                    D3D11_SDK_VERSION, &desc, &swap_chain, &device, nullptr, &context);
        if (FAILED(result))
            result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
                                                   D3D11_SDK_VERSION, &desc, &swap_chain, &device, nullptr, &context);
        return SUCCEEDED(result) && resize();
    }

    bool resize() {
        ComPtr<ID3D11Texture2D> buffer;
        return SUCCEEDED(swap_chain->GetBuffer(0, IID_PPV_ARGS(&buffer))) &&
               SUCCEEDED(device->CreateRenderTargetView(buffer.Get(), nullptr, &target));
    }

    bool resize(UINT width, UINT height) {
        context->OMSetRenderTargets(0, nullptr, nullptr);
        target.Reset();
        return SUCCEEDED(swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0)) && resize();
    }
};

UINT pending_width = 0, pending_height = 0;
float pending_dpi = 0;
constexpr UINT tray_message = WM_APP + 1, restore_message = WM_APP + 2, assistant_message = WM_APP + 3;
constexpr wchar_t window_class_name[] = L"CyMCPHubWindow";
constexpr wchar_t run_key[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
UINT taskbar_created = 0;
bool tray_installed = false;
std::wstring startup_command;

void show_window(HWND window) {
    ShowWindow(window, IsIconic(window) ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(window);
}

bool update_tray(HWND window, DWORD action) {
    NOTIFYICONDATAW icon{};
    icon.cbSize = sizeof(icon);
    icon.hWnd = window;
    icon.uID = 1;
    icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    icon.uCallbackMessage = tray_message;
    icon.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(icon.szTip, L"CyMCPHub");
    return Shell_NotifyIconW(action, &icon) != FALSE;
}

bool startup_enabled() {
    DWORD length = 0;
    return RegGetValueW(HKEY_CURRENT_USER, run_key, L"CyMCPHub", RRF_RT_REG_SZ, nullptr, nullptr, &length) == ERROR_SUCCESS;
}

bool set_startup(bool enabled) {
    HKEY key = nullptr;
    if (enabled) {
        if (startup_command.empty() || startup_command.size() >= 260 ||
            RegCreateKeyExW(HKEY_CURRENT_USER, run_key, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
            return false;
    } else if (RegOpenKeyExW(HKEY_CURRENT_USER, run_key, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }
    const auto status = enabled ? RegSetValueExW(key, L"CyMCPHub", 0, REG_SZ, reinterpret_cast<const BYTE*>(startup_command.c_str()),
                              static_cast<DWORD>((startup_command.size() + 1) * sizeof(wchar_t))) : RegDeleteValueW(key, L"CyMCPHub");
    RegCloseKey(key);
    return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
}

std::wstring quote_path(const hub::fs::path& path) {
    auto text = path.native();
    const auto last_character = text.find_last_not_of(L'\\');
    if (last_character != std::wstring::npos) text.append(text.size() - 1 - last_character, L'\\');
    return L'"' + text + L'"';
}

void prepare_startup(const hub::Options& options) {
    std::wstring executable(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (!length || length >= executable.size()) return;
    executable.resize(length);
    startup_command = quote_path(executable) + L" --tray --workspace " + quote_path(options.workspace);
    if (!options.extra_config.empty()) startup_command += L" --config " + quote_path(options.extra_config);
    if (!options.scan_ports) startup_command += L" --no-port-scan";
}

void tray_menu(HWND window) {
    const bool french = PRIMARYLANGID(LANGIDFROMLCID(GetUserDefaultLCID())) == LANG_FRENCH;
    const bool startup = startup_enabled();
    const auto menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, 1, french ? L"Ouvrir CyMCPHub" : L"Open CyMCPHub");
    AppendMenuW(menu, MF_STRING, 4, french ? L"Ouvrir l’assistant" : L"Open Assistant");
    AppendMenuW(menu, MF_STRING | (startup ? MF_CHECKED : MF_UNCHECKED), 2,
                french ? L"Démarrer avec Windows" : L"Start with Windows");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 3, french ? L"Quitter CyMCPHub" : L"Quit CyMCPHub");
    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(window);
    const auto command = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, cursor.x, cursor.y, 0, window, nullptr);
    DestroyMenu(menu);
    PostMessageW(window, WM_NULL, 0, 0);
    if (command == 1) show_window(window);
    else if (command == 4) PostMessageW(window, assistant_message, 0, 0);
    else if (command == 2 && !set_startup(!startup))
        MessageBoxW(window, french ? L"Impossible de modifier le démarrage automatique." : L"Could not change automatic startup.",
                    L"CyMCPHub", MB_OK | MB_ICONWARNING);
    else if (command == 3) DestroyWindow(window);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (taskbar_created && message == taskbar_created) {
        tray_installed = update_tray(window, NIM_ADD);
        if (!tray_installed) show_window(window);
        return 0;
    }
    if (ImGui_ImplWin32_WndProcHandler(window, message, wparam, lparam)) return 1;
    switch (message) {
    case tray_message:
        if (lparam == WM_LBUTTONUP) show_window(window);
        else if (lparam == WM_RBUTTONUP || lparam == WM_CONTEXTMENU) tray_menu(window);
        return 0;
    case restore_message:
        show_window(window);
        return 0;
    case assistant_message:
        hub::tray_assistant_requested = true;
        show_window(window);
        return 0;
    case WM_CLOSE:
        if (tray_installed) ShowWindow(window, SW_HIDE);
        else DestroyWindow(window);
        return 0;
    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED) {
            pending_width = LOWORD(lparam);
            pending_height = HIWORD(lparam);
        }
        return 0;
    case WM_GETMINMAXINFO: {
        auto& info = *reinterpret_cast<MINMAXINFO*>(lparam);
        const auto dpi = ImGui_ImplWin32_GetDpiScaleForHwnd(window);
        info.ptMinTrackSize = {static_cast<LONG>(860 * dpi), static_cast<LONG>(610 * dpi)};
        return 0;
    }
    case WM_DPICHANGED: {
        pending_dpi = static_cast<float>(HIWORD(wparam)) / 96.0f;
        const auto& rect = *reinterpret_cast<RECT*>(lparam);
        SetWindowPos(window, nullptr, rect.left, rect.top, rect.right - rect.left,
                     rect.bottom - rect.top, SWP_NOACTIVATE | SWP_NOZORDER);
        return 0;
    }
    case WM_SYSCOMMAND:
        if ((wparam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        if (tray_installed) update_tray(window, NIM_DELETE);
        tray_installed = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
} // namespace

namespace hub {
void run_window(HWND window, Options options) {
    Renderer renderer;
    if (!renderer.create(window)) throw std::runtime_error("Direct3D initialization failed");
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_ViewportsEnable;
    cyui::Config config;
    config.themeKey = "amber-night";
    config.density = cyui::Density::comfortable;
    config.dpiScale = ImGui_ImplWin32_GetDpiScaleForHwnd(window);
    cyui::apply(config);
    cyui::loadFonts();
    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(renderer.device.Get(), renderer.context.Get());

    bool occluded = false;
    draw_hub(window, std::move(options), [&](const Draw& draw, const Draw& draw_assistant, bool& assistant_open, bool&) {
        MSG event{};
        while (PeekMessageW(&event, nullptr, 0, 0, PM_REMOVE)) {
            if (event.message == WM_QUIT) return false;
            TranslateMessage(&event);
            DispatchMessageW(&event);
        }
        const bool main_hidden = !IsWindowVisible(window) || IsIconic(window);
        const bool has_secondary = ImGui::GetPlatformIO().Viewports.Size > 1;
        const bool main_occluded = !main_hidden && occluded && renderer.swap_chain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED;
        if ((main_hidden || main_occluded) && !has_secondary && !tray_assistant_requested) {
            MsgWaitForMultipleObjects(0, nullptr, FALSE, 250, QS_ALLINPUT);
            return true;
        }
        if (pending_width && pending_height) {
            if (!renderer.resize(pending_width, pending_height)) return false;
            pending_width = pending_height = 0;
        }
        if (pending_dpi) {
            cyui::applyDpi(pending_dpi);
            pending_dpi = 0;
        }
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        draw();
        if (assistant_open) draw_assistant();
        cyui::renderNotifications();
        ImGui::Render();
        if (!main_hidden && !main_occluded) {
            const ImVec4 clear = ImGui::ColorConvertU32ToFloat4(cyui::palette().bg0);
            renderer.context->OMSetRenderTargets(1, renderer.target.GetAddressOf(), nullptr);
            renderer.context->ClearRenderTargetView(renderer.target.Get(), &clear.x);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            const auto result = renderer.swap_chain->Present(1, 0);
            occluded = result == DXGI_STATUS_OCCLUDED;
            if (FAILED(result)) return false;
        }
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        if (main_hidden || main_occluded) MsgWaitForMultipleObjects(0, nullptr, FALSE, 16, QS_ALLINPUT);
        return true;
    });

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}
} // namespace hub

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    try {
        int argc = 0;
        auto** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (!argv) return 1;
        const std::unique_ptr<void, decltype(&LocalFree)> buffer(argv, LocalFree);
        std::vector<std::string> arguments;
        for (int i = 0; i < argc; ++i) arguments.push_back(cymcp::path_utf8(hub::fs::path(argv[i])));
        auto args = hub::parse_arguments(arguments);
        if (args.help || args.list) {
            MessageBoxW(nullptr, L"CyMCPHub [--tray] [--workspace PATH] [--config FILE] [--no-port-scan]\n"
                                 L"CyMCPHubMcp --list exports the inventory.", L"CyMCPHub", MB_OK);
            return 0;
        }

        const auto raw_instance = CreateMutexW(nullptr, FALSE, L"Local\\CyMCPHub.SingleInstance");
        const auto existing = GetLastError() == ERROR_ALREADY_EXISTS;
        const std::unique_ptr<void, decltype(&CloseHandle)> single_instance(raw_instance, CloseHandle);
        if (raw_instance && existing) {
            for (int attempt = 0; attempt < 20; ++attempt) {
                if (const auto window = FindWindowW(window_class_name, nullptr)) {
                    if (!args.tray) PostMessageW(window, restore_message, 0, 0);
                    return 0;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            return 1;
        }
        taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
        prepare_startup(args.options);

        ImGui_ImplWin32_EnableDpiAwareness();
        const auto monitor = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        GetMonitorInfoW(monitor, &info);
        const auto dpi = ImGui_ImplWin32_GetDpiScaleForMonitor(monitor);
        const LONG width = std::min(static_cast<LONG>(1200 * dpi), info.rcWork.right - info.rcWork.left);
        const LONG height = std::min(static_cast<LONG>(790 * dpi), info.rcWork.bottom - info.rcWork.top);
        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.style = CS_CLASSDC;
        window_class.lpfnWndProc = window_proc;
        window_class.hInstance = instance;
        window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        window_class.lpszClassName = window_class_name;
        if (!RegisterClassExW(&window_class)) return 1;
        const auto window = CreateWindowW(window_class.lpszClassName, L"CyMCPHub", WS_OVERLAPPEDWINDOW,
                                          (info.rcWork.left + info.rcWork.right - width) / 2,
                                          (info.rcWork.top + info.rcWork.bottom - height) / 2,
                                          width, height, nullptr, nullptr, instance, nullptr);
        if (!window) return 1;
        tray_installed = update_tray(window, NIM_ADD);
        if (!args.tray || !tray_installed) ShowWindow(window, show);
        hub::run_window(window, std::move(args.options));
        if (IsWindow(window)) DestroyWindow(window);
        UnregisterClassW(window_class.lpszClassName, instance);
        return 0;
    } catch (const std::exception&) {
        MessageBoxW(nullptr, L"CyMCPHub could not start. Check the command-line options and Direct3D support.",
                    L"CyMCPHub", MB_OK | MB_ICONERROR);
        return 1;
    }
}
