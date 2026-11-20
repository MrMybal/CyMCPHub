// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "platform.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <wbemidl.h>
#include <winternl.h>
#include <wrl/client.h>

#include <algorithm>
#include <bit>

namespace hub {
namespace {
using Microsoft::WRL::ComPtr;

struct Bstr {
    BSTR value;
    explicit Bstr(const wchar_t* text) : value(SysAllocString(text)) {}
    ~Bstr() { SysFreeString(value); }
};

struct Variant {
    VARIANT value;
    Variant() { VariantInit(&value); }
    ~Variant() { VariantClear(&value); }
};

struct ComApartment {
    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ~ComApartment() { if (SUCCEEDED(result)) CoUninitialize(); }
};

std::string utf8(std::wstring_view value);

void append_process(std::vector<Process>& processes, DWORD pid, const wchar_t* text) {
    int argc = 0;
    if (!text || !*text) return;
    auto argv = CommandLineToArgvW(text, &argc);
    if (!argv) return;
    const std::unique_ptr<void, decltype(&LocalFree)> argument_buffer(argv, LocalFree);
    Process process{pid, {}};
    for (int i = 0; i < argc; ++i) process.arguments.push_back(utf8(argv[i]));
    processes.push_back(std::move(process));
}

std::vector<Process> process_snapshot(const cymcp::Cancellation& cancellation, bool& available) {
    using QueryProcess = NTSTATUS (NTAPI*)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);
    const auto nt = GetModuleHandleW(L"ntdll.dll");
    const auto address = nt ? GetProcAddress(nt, "NtQueryInformationProcess") : nullptr;
    if (!address) return {};
    const auto query = std::bit_cast<QueryProcess>(address);
    // Windows 10+ supplies ProcessCommandLineInformation as a self-contained UNICODE_STRING.
    constexpr auto command_line_information = static_cast<PROCESSINFOCLASS>(60);
    const auto raw_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (raw_snapshot == INVALID_HANDLE_VALUE) return {};
    const std::unique_ptr<void, decltype(&CloseHandle)> snapshot(raw_snapshot, CloseHandle);
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    std::vector<Process> processes;
    if (!Process32FirstW(snapshot.get(), &entry)) return processes;
    do {
        cancellation.check();
        const std::unique_ptr<void, decltype(&CloseHandle)> process(
            OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID), CloseHandle);
        if (!process) continue;
        ULONG size = 0;
        query(process.get(), command_line_information, nullptr, 0, &size);
        if (size < sizeof(UNICODE_STRING) || size > 131072) continue;
        std::vector<std::byte> buffer(size);
        if (query(process.get(), command_line_information, buffer.data(), size, &size) < 0) continue;
        const auto& command = *reinterpret_cast<const UNICODE_STRING*>(buffer.data());
        const auto address_of_buffer = reinterpret_cast<std::uintptr_t>(buffer.data());
        const auto address_of_text = reinterpret_cast<std::uintptr_t>(command.Buffer);
        if (command.Length % sizeof(wchar_t) || address_of_text < address_of_buffer ||
            address_of_text - address_of_buffer > buffer.size() ||
            command.Length > buffer.size() - (address_of_text - address_of_buffer)) continue;
        const std::wstring text(command.Buffer, command.Length / sizeof(wchar_t));
        append_process(processes, entry.th32ProcessID, text.c_str());
    } while (Process32NextW(snapshot.get(), &entry));
    available = !processes.empty();
    return processes;
}

std::vector<std::byte> tcp_table(ULONG family) {
    ULONG size = 0;
    auto result = GetExtendedTcpTable(nullptr, &size, FALSE, family, TCP_TABLE_OWNER_PID_LISTENER, 0);
    for (int attempt = 0; attempt < 4 && result == ERROR_INSUFFICIENT_BUFFER; ++attempt) {
        std::vector<std::byte> table(size);
        result = GetExtendedTcpTable(table.data(), &size, FALSE, family, TCP_TABLE_OWNER_PID_LISTENER, 0);
        if (result == NO_ERROR) return table;
    }
    return {};
}
} // namespace

fs::path environment_path(const char* name) {
    const auto wide = from_utf8(name);
    const auto needed = GetEnvironmentVariableW(wide.c_str(), nullptr, 0);
    if (!needed) return {};
    std::wstring value(needed, L'\0');
    const auto written = GetEnvironmentVariableW(wide.c_str(), value.data(), needed);
    if (written >= needed) return {};
    value.resize(written);
    return value;
}

namespace {
std::string utf8(std::wstring_view value) {
    if (value.empty()) return {};
    const int length = static_cast<int>(value.size());
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), length, nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), length, result.data(), size, nullptr, nullptr);
    return result;
}
} // namespace

fs::path home_path() { return environment_path("USERPROFILE"); }
fs::path client_config_path() { return environment_path("APPDATA"); }

fs::path data_path() {
    if (const auto override = environment_path("CYMCP_HUB_DATA"); !override.empty()) return override;
    const auto local = environment_path("LOCALAPPDATA");
    if (local.empty()) throw std::runtime_error("LOCALAPPDATA is unavailable");
    return local / "CyMCPHub";
}

fs::path executable_path() {
    std::wstring path(32768, L'\0');
    const DWORD size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!size || size >= path.size()) return {};
    path.resize(size);
    return path;
}

bool same_path(const fs::path& left, const fs::path& right) {
    return CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
}

bool path_is_link(const fs::path& path) {
    const auto attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT);
}

unsigned long process_id() { return GetCurrentProcessId(); }

bool french_locale() { return PRIMARYLANGID(LANGIDFROMLCID(GetUserDefaultLCID())) == LANG_FRENCH; }

FileLock::FileLock(const fs::path& path) {
    const auto handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot access data lock");
    OVERLAPPED range{};
    for (int tries = 0; tries < 40; ++tries) {
        if (LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &range)) {
            handle_ = reinterpret_cast<std::intptr_t>(handle);
            return;
        }
        if (GetLastError() != ERROR_LOCK_VIOLATION) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    CloseHandle(handle);
    throw std::runtime_error("Data store is busy");
}

FileLock::~FileLock() { CloseHandle(reinterpret_cast<HANDLE>(handle_)); }

std::error_code publish_file(const fs::path& temporary, const fs::path& destination, bool replace) {
    if (MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0))) return {};
    const auto error = GetLastError();
    if (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS) return std::make_error_code(std::errc::file_exists);
    return {static_cast<int>(error), std::system_category()};
}

std::string group_key(std::string_view name) {
    const auto wide = from_utf8(name).native();
    if (wide.empty()) return {};
    const auto size = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, wide.data(), static_cast<int>(wide.size()),
                                    nullptr, 0, nullptr, nullptr, 0);
    if (!size) return std::string(name);
    std::wstring folded(size, L'\0');
    if (!LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, wide.data(), static_cast<int>(wide.size()),
                       folded.data(), size, nullptr, nullptr, 0)) return std::string(name);
    return utf8(folded);
}

std::vector<Process> wmi_processes(const cymcp::Cancellation& cancellation, bool& available) {
    available = false;
    ComApartment apartment;
    if (FAILED(apartment.result) && apartment.result != RPC_E_CHANGED_MODE) return {};

    ComPtr<IWbemLocator> locator;
    if (FAILED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&locator)))) return {};
    Bstr ns(L"ROOT\\CIMV2");
    ComPtr<IWbemServices> services;
    if (FAILED(locator->ConnectServer(ns.value, nullptr, nullptr, nullptr,
                                      WBEM_FLAG_CONNECT_USE_MAX_WAIT, nullptr, nullptr, &services))) return {};
    if (FAILED(CoSetProxyBlanket(services.Get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                                 RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE))) return {};

    Bstr language(L"WQL"), query(L"SELECT ProcessId, CommandLine FROM Win32_Process");
    ComPtr<IEnumWbemClassObject> rows;
    if (FAILED(services->ExecQuery(language.value, query.value,
                                   WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &rows))) return {};

    std::vector<Process> processes;
    const auto deadline = cymcp::Clock::now() + std::chrono::seconds(3);
    while (cymcp::Clock::now() < deadline) {
        cancellation.check();
        ComPtr<IWbemClassObject> row;
        ULONG count = 0;
        const auto result = rows->Next(200, 1, row.GetAddressOf(), &count);
        if (result == WBEM_S_FALSE) {
            available = true;
            break;
        }
        if (FAILED(result)) break;
        if (!count) continue;

        Variant command, pid;
        if (FAILED(row->Get(L"CommandLine", 0, &command.value, nullptr, nullptr)) ||
            command.value.vt != VT_BSTR || !command.value.bstrVal || !*command.value.bstrVal ||
            FAILED(row->Get(L"ProcessId", 0, &pid.value, nullptr, nullptr)) ||
            (pid.value.vt != VT_I4 && pid.value.vt != VT_UI4)) continue;
        const auto process_id = pid.value.vt == VT_I4 ? static_cast<DWORD>(pid.value.lVal) : pid.value.ulVal;
        append_process(processes, process_id, command.value.bstrVal);
    }
    return processes;
}

std::vector<Process> running_processes(const cymcp::Cancellation& cancellation, bool& available) {
    auto processes = wmi_processes(cancellation, available);
    if (available) return processes;
    auto fallback = process_snapshot(cancellation, available);
    return available ? std::move(fallback) : std::move(processes);
}

std::vector<Listener> local_listeners(bool& available) {
    const auto v4 = tcp_table(AF_INET);
    const auto v6 = tcp_table(AF_INET6);
    available = !v4.empty() && !v6.empty();

    std::vector<Listener> listeners;
    if (!v4.empty()) {
        const auto& table = *reinterpret_cast<const MIB_TCPTABLE_OWNER_PID*>(v4.data());
        for (DWORD i = 0; i < table.dwNumEntries; ++i) {
            const auto& row = table.table[i];
            if (row.dwLocalAddr != 0 && ntohl(row.dwLocalAddr) != INADDR_LOOPBACK) continue;
            listeners.push_back({"http://127.0.0.1:" +
                                 std::to_string(ntohs(static_cast<u_short>(row.dwLocalPort))) + "/mcp", row.dwOwningPid});
        }
    }
    if (!v6.empty()) {
        const auto& table = *reinterpret_cast<const MIB_TCP6TABLE_OWNER_PID*>(v6.data());
        const unsigned char loopback[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
        for (DWORD i = 0; i < table.dwNumEntries; ++i) {
            const auto& row = table.table[i];
            if (!std::all_of(std::begin(row.ucLocalAddr), std::end(row.ucLocalAddr), [](auto byte) { return byte == 0; }) &&
                !std::equal(std::begin(row.ucLocalAddr), std::end(row.ucLocalAddr), std::begin(loopback))) continue;
            listeners.push_back({"http://[::1]:" +
                                 std::to_string(ntohs(static_cast<u_short>(row.dwLocalPort))) + "/mcp", row.dwOwningPid});
        }
    }
    return listeners;
}
} // namespace hub
