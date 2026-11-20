// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "catalog.hpp"
#include "mcp_actions.hpp"
#include "platform.hpp"

#include <cymcp/http.hpp>
#include <toml++/toml.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>

namespace hub {
namespace {
struct Entry {
    Server server;
    std::string url, command;
    std::vector<std::string> args;
    Json configuration;
};

std::string lower(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

std::string field(const Json& value, const char* key) {
    const auto found = value.find(key);
    return found != value.end() && found->is_string() ? found->get<std::string>() : std::string{};
}

std::string read_file(const fs::path& path) {
    constexpr auto limit = 4 * 1024 * 1024;
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open file");
    std::string bytes(limit + 1, '\0');
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (input.bad() || input.gcount() > limit) throw std::runtime_error("Cannot read file");
    bytes.resize(static_cast<std::size_t>(input.gcount()));
    return bytes;
}

std::string normalize_jsonc(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    bool quoted = false;
    auto last = std::string::npos;
    for (std::size_t i = 0; i < input.size(); ++i) {
        const char c = input[i];
        if (quoted) {
            output += c;
            if (c == '\\' && i + 1 < input.size()) output += input[++i];
            else if (c == '"') {
                quoted = false;
                last = output.size() - 1;
            }
        } else if (c == '/' && i + 1 < input.size() && input[i + 1] == '/') {
            i = input.find_first_of("\r\n", i + 2);
            if (i == std::string::npos) break;
            output += '\n';
        } else if (c == '/' && i + 1 < input.size() && input[i + 1] == '*') {
            const auto end = input.find("*/", i + 2);
            if (end == std::string::npos) throw std::runtime_error("Unterminated comment");
            i = end + 1;
            output += ' ';
        } else {
            if ((c == '}' || c == ']') && last != std::string::npos && output[last] == ',') output[last] = ' ';
            output += c;
            if (!std::isspace(static_cast<unsigned char>(c))) last = output.size() - 1;
            quoted = c == '"';
        }
    }
    return output;
}

std::string public_url(std::string url) {
    const auto scheme = url.find("://");
    if (scheme == std::string::npos) return "<configured value>";
    const auto start = scheme + 3;
    const auto end = url.find_first_of("/?#", start);
    const auto auth = url.rfind('@', end);
    if (auth != std::string::npos && auth >= start) url.replace(start, auth - start, "<redacted>");
    const auto query = url.find_first_of("?#", start);
    if (query != std::string::npos) url.replace(query + 1, std::string::npos, "<redacted>");
    return url;
}

bool sensitive_flag(std::string key) {
    key = lower(std::move(key));
    return key == "-h" || key == "-e" || key == "-u" ||
           key.find("token") != std::string::npos || key.find("password") != std::string::npos ||
           key.find("secret") != std::string::npos || key.find("api-key") != std::string::npos ||
           key.find("api_key") != std::string::npos || key.find("apikey") != std::string::npos ||
           key.find("credential") != std::string::npos || key.find("header") != std::string::npos ||
           key.find("authorization") != std::string::npos || key.find("cookie") != std::string::npos ||
           key == "--key" || key == "--auth" || key == "--env";
}

Json public_arguments(const std::vector<std::string>& args) {
    Json safe = Json::array();
    bool mask_next = false;
    for (const auto& arg : args) {
        if (mask_next) {
            safe.push_back("<redacted>");
            mask_next = false;
            continue;
        }
        const auto delimiter = arg.find_first_of("= \t:");
        const auto key = arg.substr(0, delimiter);
        const bool flag = !arg.empty() && arg.front() == '-';
        if (arg.size() > 2 && (arg.starts_with("-H") || arg.starts_with("-e") || arg.starts_with("-u"))) {
            safe.push_back(arg.substr(0, 2) + "<redacted>");
        } else if (flag && sensitive_flag(key)) {
            safe.push_back(delimiter == std::string::npos ? key : key + "=<redacted>");
            mask_next = delimiter == std::string::npos;
        } else if (delimiter != std::string::npos && !flag && sensitive_flag(key)) {
            safe.push_back(key + "=<redacted>");
        } else if (arg.starts_with("http://") || arg.starts_with("https://")) {
            safe.push_back(public_url(arg));
        } else if (const auto url = arg.find("://"); url != std::string::npos) {
            const auto start = arg.rfind('=', url);
            safe.push_back(start == std::string::npos ? "<redacted>" :
                           arg.substr(0, start + 1) + public_url(arg.substr(start + 1)));
        } else {
            safe.push_back(arg);
        }
    }
    return safe;
}

Json keys(const Json& object) {
    auto names = Json::array();
    if (object.is_object()) for (const auto& [name, value] : object.items()) names.push_back(name);
    if (object.is_array()) for (const auto& value : object) if (value.is_string()) names.push_back(value);
    return names;
}

Json as_json(toml::node_view<const toml::node> node) {
    if (const auto* table = node.as_table()) {
        auto result = Json::object();
        for (const auto& [key, value] : *table) result[std::string(key.str())] = as_json(toml::node_view<const toml::node>{value});
        return result;
    }
    if (const auto* array = node.as_array()) {
        auto result = Json::array();
        for (const auto& value : *array) result.push_back(as_json(toml::node_view<const toml::node>{value}));
        return result;
    }
    if (const auto* value = node.as_string()) return value->get();
    if (const auto* value = node.as_boolean()) return value->get();
    return nullptr;
}

void append_entries(const Json& entries, const std::string& source, const fs::path& file,
                    std::vector<Entry>& result) {
    if (!entries.is_object()) return;
    for (const auto& [name, config] : entries.items()) {
        if (!config.is_object()) continue;
        Entry entry;
        entry.configuration = config;
        auto& server = entry.server;
        server.name = name;
        server.source = source;
        server.source_file = file;
        server.id = cymcp::sha256(cymcp::path_utf8(file) + '\n' + source + '\n' + name).substr(0, 24);
        const auto type = field(config, "type");
        entry.url = field(config, "url");
        if (entry.url.empty()) entry.url = field(config, "serverUrl");
        if (entry.url.empty()) entry.url = field(config, "httpUrl");
        entry.command = field(config, "command");
        const auto arguments = config.find("args");
        if (arguments != config.end() && arguments->is_array()) {
            for (const auto& arg : *arguments) if (arg.is_string()) entry.args.push_back(arg.get<std::string>());
        } else if (const auto command = config.find("command"); command != config.end() &&
                   command->is_array() && !command->empty() && command->front().is_string()) {
            entry.command = command->front();
            for (std::size_t i = 1; i < command->size(); ++i)
                if ((*command)[i].is_string()) entry.args.push_back((*command)[i].get<std::string>());
        }

        if (!entry.url.empty()) {
            server.transport = type == "sse" ? "sse" : "http";
            server.connection["url"] = public_url(entry.url);
            auto headers = keys(config.value("headers", Json()));
            for (const auto& header : keys(config.value("env_http_headers", Json()))) headers.push_back(header);
            if (!headers.empty()) server.connection["headerNames"] = headers;
            if (auto bearer = field(config, "bearer_token_env_var"); !bearer.empty())
                server.connection["bearerTokenEnvVar"] = bearer;
        } else if (!entry.command.empty()) {
            server.transport = "stdio";
            server.connection = {{"command", entry.command}, {"args", public_arguments(entry.args)}};
            auto environment = keys(config.value("env", Json()));
            for (const auto& key : keys(config.value("env_vars", Json()))) environment.push_back(key);
            for (const auto& key : keys(config.value("environment", Json()))) environment.push_back(key);
            if (!environment.empty()) server.connection["environmentNames"] = environment;
            if (auto cwd = field(config, "cwd"); !cwd.empty()) server.connection["cwd"] = cwd;
        } else {
            continue;
        }
        if (config.value("enabled", Json()) == false || config.value("disabled", Json()) == true)
            server.status = Status::disabled;
        result.push_back(std::move(entry));
    }
}

void load_source(const fs::path& file, const std::string& source, const fs::path& workspace,
                 std::vector<Entry>& result, Snapshot& snapshot, bool required = false) {
    std::error_code error;
    if (!fs::exists(file, error) && !error && !required) return;
    try {
        const auto bytes = read_file(file);
        if (group_key(cymcp::path_utf8(file.extension())) == ".toml") {
            const auto data = toml::parse(bytes);
            append_entries(as_json(data["mcp_servers"]), source, file, result);
            return;
        }
        const auto data = Json::parse(normalize_jsonc(bytes));
        if (!data.is_object()) throw std::runtime_error("Expected an object");
        for (const auto* key : {"mcpServers", "servers", "mcp"})
            if (const auto found = data.find(key); found != data.end())
                append_entries(*found, source, file, result);
        const auto projects = data.find("projects");
        if (!workspace.empty() && projects != data.end() && projects->is_object()) {
            for (const auto& [path, project] : projects->items()) {
                if (same_path(from_utf8(path).lexically_normal(), workspace) && project.is_object())
                    append_entries(project.value("mcpServers", Json()), source + " / project", file, result);
            }
        }
    } catch (const std::exception&) {
        snapshot.unreadable_sources.push_back(file);
    }
}

std::vector<Entry> configured_entries(const Options& options, Snapshot& snapshot) {
    const auto home = home_path();
    const auto roaming = client_config_path();
    auto codex = environment_path("CODEX_HOME");
    std::vector<Entry> result;
    auto load = [&](const fs::path& file, const char* source) {
        load_source(file, source, options.workspace, result, snapshot);
    };
    if (!roaming.empty()) {
        load(roaming / "Claude/claude_desktop_config.json", "Claude Desktop");
        for (const auto& [product, label] : {std::pair{"Code", "VS Code"}, {"Code - Insiders", "VS Code Insiders"}, {"VSCodium", "VSCodium"}}) {
            const auto user = roaming / product / "User";
            load(user / "mcp.json", label);
            std::error_code error;
            fs::directory_iterator it(user / "profiles", error), end;
            for (; it != end && !error; it.increment(error)) load(it->path() / "mcp.json", label);
        }
        load(roaming / "opencode/opencode.json", "OpenCode");
        load(roaming / "opencode/opencode.jsonc", "OpenCode");
    }
    if (!home.empty()) {
        if (codex.empty()) codex = home / ".codex";
        load(home / ".claude.json", "Claude Code");
        load(home / ".cursor/mcp.json", "Cursor");
        load(home / ".gemini/settings.json", "Gemini CLI");
        load(home / ".codeium/windsurf/mcp_config.json", "Windsurf");
        if (!same_path(roaming, home / ".config")) {
            load(home / ".config/opencode/opencode.json", "OpenCode");
            load(home / ".config/opencode/opencode.jsonc", "OpenCode");
        }
    }
    if (!codex.empty()) load(codex / "config.toml", "Codex");
    if (!options.workspace.empty()) {
        const auto& root = options.workspace;
        load(root / ".mcp.json", "Claude Code");
        load(root / ".vscode/mcp.json", "VS Code");
        load(root / ".cursor/mcp.json", "Cursor");
        load(root / ".codex/config.toml", "Codex");
        load(root / ".gemini/settings.json", "Gemini CLI");
        load(root / "opencode.json", "OpenCode");
        load(root / "opencode.jsonc", "OpenCode");
    }
    if (!options.extra_config.empty())
        load_source(options.extra_config, "Custom config", options.workspace, result, snapshot, true);
    return result;
}

bool command_matches(const Entry& entry, const Process& process) {
    const auto command = from_utf8(entry.command);
    const auto argument_count = entry.args.size() + 1;
    if (process.arguments.size() < argument_count) return false;
    const auto offset = process.arguments.size() - argument_count;
    const auto actual = from_utf8(process.arguments[offset]);
    const auto wanted = command.has_parent_path() ? command.lexically_normal() : command.filename();
    const auto running = command.has_parent_path() ? actual.lexically_normal() : actual.filename();
#ifdef _WIN32
    if (offset && !same_path(from_utf8(process.arguments.front()).filename(), "cmd.exe")) return false;
    const auto extension = lower(cymcp::path_utf8(running.extension()));
    const bool launcher_extension = extension == ".exe" || extension == ".cmd" || extension == ".bat";
    if (!same_path(wanted, running) &&
        (wanted.has_extension() || !launcher_extension ||
         !same_path(wanted, running.parent_path() / running.stem()))) return false;
#else
    if (!same_path(wanted, running)) return false;
    if (offset) {
        // A script launched through a shebang appears after its interpreter in argv.
        std::error_code error;
        if (offset > 2 || !actual.is_absolute() || !fs::is_regular_file(actual, error)) return false;
        std::ifstream script(actual, std::ios::binary);
        char prefix[2]{};
        if (!script.read(prefix, sizeof(prefix)) || prefix[0] != '#' || prefix[1] != '!') return false;
    }
#endif
    for (std::size_t i = 0; i < entry.args.size(); ++i)
        if (entry.args[i] != process.arguments[offset + i + 1]) return false;
    return true;
}

Json rpc_result(const std::string& body) {
    auto extract = [](const std::string& payload) -> Json {
        const auto message = Json::parse(payload, nullptr, false);
        if (message.is_object() && message.value("jsonrpc", Json()) == "2.0" && message.value("id", Json()) == 1)
            return message.value("result", Json());
        return nullptr;
    };
    const auto first = body.find_first_not_of(" \r\n\t");
    if (first != std::string::npos && body[first] == '{') return extract(body);
    std::istringstream lines(body);
    std::string line, data;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.starts_with("data:")) data += line.substr(5) + '\n';
        else if (line.empty() && !data.empty()) {
            if (auto result = extract(data); result.is_object()) return result;
            data.clear();
        }
    }
    return nullptr;
}

void verify_http(Entry& entry, const cymcp::Cancellation& cancellation) {
    if (entry.server.status == Status::disabled || entry.server.transport != "http") return;
    try {
        if (!cymcp::parse_url(entry.url).loopback) return;
    } catch (const cymcp::Error&) {
        return;
    }

    auto& server = entry.server;
    server.status = Status::unverified;
    cymcp::HttpRequest request;
    request.method = "POST";
    request.url = entry.url;
    request.headers = {{"Content-Type", "application/json"}, {"Accept", "application/json, text/event-stream"}};
    request.timeout = std::chrono::milliseconds(server.source_file.empty() ? 800 : 1800);
    request.max_response_bytes = 64 * 1024;
    request.error_body_limit = 4096;
    request.body = Json({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
                         {"params", {{"protocolVersion", "2025-11-25"}, {"capabilities", Json::object()},
                                     {"clientInfo", {{"name", "CyMCPHub"}, {"version", CYMCP_HUB_VERSION}}}}}}).dump();

    std::string received, session;
    Json result;
    try {
        const auto response = cymcp::HttpClient().perform(request, cancellation, [&](const char* data, std::size_t size) {
            received.append(data, size);
            if (received.find("\n\n") != std::string::npos || received.find("\r\n\r\n") != std::string::npos)
                result = rpc_result(received);
            return !result.is_object();
        });
        if (response.status == 401 || response.status == 403) server.status = Status::authentication;
        if (response.status < 200 || response.status >= 300) return;
        result = rpc_result(received);
        if (const auto found = response.headers.find("mcp-session-id"); found != response.headers.end()) session = found->second;
    } catch (const cymcp::Error&) {
        if (!result.is_object()) return;
    }

    const auto identity = result.find("serverInfo");
    const auto protocol = field(result, "protocolVersion");
    if (identity == result.end() || !identity->is_object() || !result.value("capabilities", Json()).is_object() ||
        protocol.empty() || field(*identity, "name").empty() || field(*identity, "version").empty()) return;

    server.server_name = field(*identity, "name");
    server.version = field(*identity, "version");
    server.protocol = protocol;
    server.summary = cymcp::redact_error_message(field(result, "instructions"));
    server.status = Status::verified;
    if (server.source_file.empty()) server.name = server.server_name;
    if (!session.empty()) {
        request.method = "DELETE";
        request.body.clear();
        request.timeout = std::chrono::milliseconds(250);
        request.headers["Mcp-Session-Id"] = session;
        request.headers["MCP-Protocol-Version"] = protocol;
        try { (void)cymcp::HttpClient().perform(request, cancellation); }
        catch (const cymcp::Error&) {}
    }
}

std::string endpoint_key(const std::string& url) {
    try {
        const auto parsed = cymcp::parse_url(url);
        const auto host = parsed.host == "localhost" ? "127.0.0.1" : parsed.host;
        return parsed.scheme + '|' + host + '|' + std::to_string(parsed.port) + '|' + parsed.target;
    } catch (const cymcp::Error&) { return url; }
}

std::string environment_value(const std::string& name) {
    if (name.empty() || name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos)
        throw cymcp::Error("UnsupportedVariable", "Unsupported variable in the MCP configuration");
    return cymcp::path_utf8(environment_path(name.c_str()));
}

std::string expand_config(std::string value, const Options& options) {
    for (std::size_t pos = 0; (pos = value.find('{', pos)) != std::string::npos;) {
        const bool dollar = pos && value[pos - 1] == '$';
        const auto closing = value.find('}', pos + 1);
        if (closing == std::string::npos) break;
        auto key = value.substr(pos + 1, closing - pos - 1);
        if (!dollar && !key.starts_with("env:")) { ++pos; continue; }
        std::string replacement;
        if (key == "workspaceFolder" || key == "workspaceRoot") replacement = cymcp::path_utf8(options.workspace);
        else if (key == "userHome") replacement = cymcp::path_utf8(home_path());
        else {
            if (key.starts_with("env:")) key.erase(0, 4);
            auto default_at = key.find(":-");
            replacement = environment_value(key.substr(0, default_at));
            if (replacement.empty()) {
                if (default_at == std::string::npos) throw cymcp::Error("MissingVariable", "Environment variable is unset: " + key);
                replacement = key.substr(default_at + 2);
            }
        }
        const auto start = pos - (dollar ? 1 : 0);
        value.replace(start, closing - start + 1, replacement);
        pos = start + replacement.size();
    }
    return value;
}

std::string bearer_token(const Json& config, const Options& options) {
    std::string authorization;
    const auto add_header = [&](const std::string& name, std::string value) {
        if (lower(name) != "authorization")
            throw cymcp::Error("UnsupportedHeader", "This connector requires a custom HTTP header");
        authorization = std::move(value);
    };
    const auto headers = config.value("headers", Json());
    if (headers.is_object()) for (const auto& [name, value] : headers.items()) {
        if (value.is_string()) add_header(name, expand_config(value.get<std::string>(), options));
    }
    const auto from_environment = config.value("env_http_headers", Json());
    if (from_environment.is_object()) for (const auto& [name, value] : from_environment.items()) {
        if (value.is_string()) add_header(name, environment_value(value.get<std::string>()));
    }
    if (const auto variable = field(config, "bearer_token_env_var"); !variable.empty()) {
        auto token = environment_value(variable);
        if (token.empty()) throw cymcp::Error("MissingVariable", "Bearer token environment variable is unset: " + variable);
        return token;
    }
    if (authorization.empty()) return {};
    if (lower(authorization.substr(0, 7)) != "bearer " || authorization.size() == 7)
        throw cymcp::Error("UnsupportedAuthentication", "This HTTP connector requires authentication other than Bearer");
    return authorization.substr(7);
}

cymcp::ProcessOptions stdio_options(const Entry& entry, const Options& options) {
    cymcp::ProcessOptions process;
    process.cwd = options.workspace;
    if (const auto cwd = field(entry.configuration, "cwd"); !cwd.empty()) process.cwd /= from_utf8(expand_config(cwd, options));
    const auto command = expand_config(entry.command, options);
    auto program = from_utf8(command);
    if (program.has_parent_path() && program.is_relative()) program = process.cwd / program;
    for (const auto& arg : entry.args) process.arguments.push_back(expand_config(arg, options));
    for (const auto* key : {"env", "environment"}) {
        const auto values = entry.configuration.value(key, Json());
        if (values.is_object()) for (const auto& [name, value] : values.items())
            if (value.is_string()) process.environment[name] = expand_config(value.get<std::string>(), options);
    }
    process.executable = program;
#ifdef _WIN32
    const auto extension = lower(cymcp::path_utf8(program.extension()));
    const auto stem = lower(cymcp::path_utf8(program.stem()));
    if ((stem == "npm" || stem == "npx") && (extension.empty() || extension == ".cmd")) {
        if (extension.empty()) program += ".cmd";
        const auto shim = cymcp::find_executable(cymcp::path_utf8(program));
        const auto script = shim.parent_path() / "node_modules/npm/bin" / (stem + "-cli.js");
        if (!fs::is_regular_file(script)) throw cymcp::Error("ExecutableMissing", "Cannot find the npm CLI script");
        const auto bundled_node = shim.parent_path() / "node.exe";
        process.executable = fs::is_regular_file(bundled_node) ? bundled_node : cymcp::find_executable("node");
        process.arguments.insert(process.arguments.begin(), cymcp::path_utf8(script));
    } else {
        if (extension == ".cmd" || extension == ".bat" || extension == ".ps1")
            throw cymcp::Error("UnsupportedLauncher", "Configure an executable or an explicit interpreter for this launcher");
    }
#endif
    return process;
}

fs::path note_path() { return data_path() / "annotations.json"; }
fs::path note_lock_path(fs::path path) { return path += ".lock"; }

Json load_notes(const fs::path& path) {
    if (!fs::exists(path)) return Json::object();
    const auto notes = Json::parse(read_file(path), nullptr, false);
    if (!notes.is_object()) throw std::runtime_error("Annotation store is unreadable");
    return notes;
}
} // namespace

Arguments parse_arguments(const std::vector<std::string>& arguments) {
    Arguments result;
    auto& options = result.options;
    options.workspace = environment_path("CYMCP_HUB_WORKSPACE");
    options.extra_config = environment_path("CYMCP_HUB_CONFIG");
    for (std::size_t i = 1; i < arguments.size(); ++i) {
        const auto& arg = arguments[i];
        if (arg == "--list") result.list = true;
        else if (arg == "--tray") result.tray = true;
        else if (arg == "--no-port-scan") options.scan_ports = false;
        else if (arg == "--help" || arg == "-h") result.help = true;
        else if ((arg == "--workspace" || arg == "--config") && i + 1 < arguments.size()) {
            (arg == "--workspace" ? options.workspace : options.extra_config) = from_utf8(arguments[++i]);
        } else throw std::runtime_error("Unknown or incomplete argument");
    }
    if (options.workspace.empty()) options.workspace = fs::current_path();
    options.workspace = fs::absolute(options.workspace).lexically_normal();
    if (options.workspace.filename().empty() && options.workspace.has_relative_path())
        options.workspace = options.workspace.parent_path();
    if (!options.extra_config.empty()) options.extra_config = fs::absolute(options.extra_config).lexically_normal();
    return result;
}

const char* status_key(Status status) {
    switch (status) {
    case Status::verified: return "verified";
    case Status::process_seen: return "process_seen";
    case Status::authentication: return "authentication_required";
    case Status::unverified: return "unverified";
    case Status::disabled: return "disabled";
    case Status::configured: return "configured";
    }
    return "configured";
}

bool is_active(Status status) { return status == Status::verified || status == Status::process_seen; }

Json Server::to_json(bool details) const {
    Json data = {{"id", id}, {"name", name}, {"groupKey", group_key(name)}, {"source", source}, {"transport", transport},
                 {"status", status_key(status)}, {"annotated", !note.empty()}};
    if (pid) data["pid"] = pid;
    if (!server_name.empty()) data["serverName"] = server_name;
    if (details) {
        if (!source_file.empty()) data["sourceFile"] = cymcp::path_utf8(source_file);
        if (!protocol.empty()) data["protocolVersion"] = protocol;
        if (!version.empty()) data["serverVersion"] = version;
        if (!summary.empty()) data["summary"] = summary;
        data["connection"] = connection;
        data["annotation"] = note;
    }
    return data;
}

bool matches(const Server& server, std::string_view query) {
    const auto needle = lower(std::string(query));
    return needle.empty() || lower(server.name).find(needle) != std::string::npos ||
           lower(server.server_name).find(needle) != std::string::npos ||
           lower(server.source).find(needle) != std::string::npos || server.transport.find(needle) != std::string::npos;
}

Snapshot scan(const Options& options, const cymcp::Cancellation& cancellation) {
    Snapshot snapshot;
    auto entries = configured_entries(options, snapshot);
    const auto processes = running_processes(cancellation, snapshot.processes_available);
    for (auto& entry : entries) {
        if (entry.server.status == Status::disabled || entry.server.transport != "stdio") continue;
        const auto found = std::find_if(processes.begin(), processes.end(), [&](const Process& process) {
            return process.pid != process_id() && command_matches(entry, process);
        });
        if (found != processes.end()) {
            entry.server.pid = found->pid;
            entry.server.status = Status::process_seen;
        }
    }

    if (options.scan_ports) {
        std::set<std::string> declared;
        for (const auto& entry : entries)
            if (!entry.url.empty() && entry.server.status != Status::disabled) declared.insert(endpoint_key(entry.url));
        for (const auto& listener : local_listeners(snapshot.ports_available)) {
            if (!declared.insert(endpoint_key(listener.url)).second) continue;
            Entry entry;
            entry.url = listener.url;
            entry.server.id = cymcp::sha256(listener.url).substr(0, 24);
            entry.server.source = "Local TCP";
            entry.server.transport = "http";
            entry.server.pid = listener.pid;
            entry.server.connection = {{"url", listener.url}};
            entries.push_back(std::move(entry));
        }
    }
    {
        std::atomic_size_t next{0};
        std::vector<std::jthread> workers;
        for (std::size_t i = 0; i < std::min<std::size_t>(8, entries.size()); ++i) {
            workers.emplace_back([&] {
                while (!cancellation.cancelled() && cymcp::Clock::now() < cancellation.deadline) {
                    const auto index = next.fetch_add(1);
                    if (index >= entries.size()) break;
                    verify_http(entries[index], cancellation);
                }
            });
        }
    }
    cancellation.check();
    const auto notes = read_notes();
    std::set<std::string> seen;
    for (auto& entry : entries) {
        auto& server = entry.server;
        if (server.source_file.empty() && server.status != Status::verified) continue;
        if (!seen.insert(server.id).second) continue;
        server.note = field(notes, server.id.c_str());
        snapshot.servers.push_back(std::move(server));
    }
    std::sort(snapshot.servers.begin(), snapshot.servers.end(), [](const Server& a, const Server& b) {
        if (a.status != b.status) return a.status < b.status;
        return a.name < b.name;
    });
    return snapshot;
}

std::unique_ptr<cymcp::McpClient> make_mcp_client(const Server& server, const Options& options,
                                                cymcp::McpClientOptions client_options) {
    if (server.status == Status::disabled) throw cymcp::Error("Disabled", "Enable this connector in its source configuration first");
    if (server.source_file.empty())
        return std::make_unique<cymcp::McpClient>(server.connection.value("url", std::string{}), "", std::move(client_options));
    Snapshot snapshot;
    std::vector<Entry> entries;
    auto source = server.source;
    if (source.ends_with(" / project")) source.resize(source.size() - std::string_view(" / project").size());
    load_source(server.source_file, source, options.workspace, entries, snapshot, true);
    const auto found = std::find_if(entries.begin(), entries.end(), [&](const Entry& entry) { return entry.server.id == server.id; });
    if (found == entries.end()) throw cymcp::Error("NotFound", "Connector is missing from its source configuration");
    if (found->server.status == Status::disabled) throw cymcp::Error("Disabled", "Enable this connector in its source configuration first");
    if (found->server.transport == "stdio") return std::make_unique<cymcp::McpClient>(stdio_options(*found, options), std::move(client_options));
    if (found->server.transport == "http")
        return std::make_unique<cymcp::McpClient>(expand_config(found->url, options), bearer_token(found->configuration, options),
                                                 std::move(client_options));
    throw cymcp::Error("UnsupportedTransport", "Legacy SSE requires a Streamable HTTP or stdio connector");
}

Json read_notes() {
    const auto path = note_path();
    if (!fs::exists(path)) return Json::object();
    const FileLock lock(note_lock_path(path));
    return load_notes(path);
}

void set_note(const std::string& id, const std::string& note) {
    if (note.size() > 8192 || note.find('\0') != std::string::npos) throw std::runtime_error("Invalid annotation");
    const auto path = note_path();
    fs::create_directories(path.parent_path());
    const FileLock lock(note_lock_path(path));
    auto notes = load_notes(path);
    if (note.empty()) notes.erase(id);
    else notes[id] = note;
    auto temporary = path;
    temporary += ".tmp";
    const auto data = notes.dump(2) + '\n';
    if (data.size() > 4 * 1024 * 1024) throw std::runtime_error("Annotation store is full");
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        file.write(data.data(), static_cast<std::streamsize>(data.size()));
        file.close();
        if (!file) throw std::runtime_error("Cannot write annotation");
    }
    if (publish_file(temporary, path, true)) {
        std::error_code error;
        fs::remove(temporary, error);
        throw std::runtime_error("Cannot save annotation");
    }
}
} // namespace hub
