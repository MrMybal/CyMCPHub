// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "autostart.hpp"
#include "platform.hpp"

#include <windows.h>

namespace hub {
namespace {
constexpr wchar_t run_key[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

std::wstring quote_path(const fs::path& path) {
    auto text = path.native();
    const auto last_character = text.find_last_not_of(L'\\');
    if (last_character != std::wstring::npos) text.append(text.size() - 1 - last_character, L'\\');
    return L'"' + text + L'"';
}
} // namespace

bool autostart_enabled() {
    DWORD size = 0;
    return RegGetValueW(HKEY_CURRENT_USER, run_key, L"CyMCPHub", RRF_RT_REG_SZ, nullptr, nullptr, &size) == ERROR_SUCCESS;
}

void set_autostart(bool enabled, const Options& options) {
    constexpr auto error = "Could not change automatic startup.";
    HKEY key = nullptr;
    if (enabled) {
        const auto executable = executable_path();
        if (executable.empty()) throw std::runtime_error(error);
        auto command = quote_path(executable) + L" --tray --workspace " + quote_path(options.workspace);
        if (!options.extra_config.empty()) command += L" --config " + quote_path(options.extra_config);
        if (!options.scan_ports) command += L" --no-port-scan";
        if (command.size() >= 260 ||
            RegCreateKeyExW(HKEY_CURRENT_USER, run_key, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
            throw std::runtime_error(error);
        const auto status = RegSetValueExW(key, L"CyMCPHub", 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()),
                                           static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(key);
        if (status != ERROR_SUCCESS) throw std::runtime_error(error);
        return;
    }
    const auto opened = RegOpenKeyExW(HKEY_CURRENT_USER, run_key, 0, KEY_SET_VALUE, &key);
    if (opened == ERROR_FILE_NOT_FOUND) return;
    if (opened != ERROR_SUCCESS) throw std::runtime_error(error);
    const auto status = RegDeleteValueW(key, L"CyMCPHub");
    RegCloseKey(key);
    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) throw std::runtime_error(error);
}
} // namespace hub
