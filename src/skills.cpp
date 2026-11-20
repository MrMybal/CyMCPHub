#include "skills.hpp"
#include "platform.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>

namespace hub {
namespace {
constexpr auto markdown_limit = 256 * 1024;

fs::path library_path() { return data_path() / "skills"; }

std::string trim(std::string value) {
    const auto start = value.find_first_not_of(" \r\n\t");
    if (start == std::string::npos) return {};
    return value.substr(start, value.find_last_not_of(" \r\n\t") - start + 1);
}

std::string unquote(std::string value) {
    if (value.size() > 1 && value.front() == '"' && value.back() == '"') {
        const auto result = Json::parse(value, nullptr, false);
        if (result.is_string()) return result.get<std::string>();
    }
    if (value.size() > 1 && ((value.front() == '\'' && value.back() == '\'') ||
                             (value.front() == '"' && value.back() == '"'))) return value.substr(1, value.size() - 2);
    return value;
}

bool valid_name(std::string_view name) {
    return !name.empty() && name.size() <= 64 && name.front() != '-' && name.back() != '-' &&
           name.find("--") == std::string_view::npos &&
           name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-") == std::string_view::npos &&
           name != "con" && name != "prn" && name != "aux" && name != "nul" &&
           !(name.size() == 4 && (name.starts_with("com") || name.starts_with("lpt")) && std::isdigit(static_cast<unsigned char>(name[3])));
}

SkillDocument parse_skill(std::string content) {
    if (content.size() > markdown_limit || content.find('\0') != std::string::npos)
        throw cymcp::Error("InvalidSkill", "SKILL.md must be UTF-8 Markdown up to 256 KiB");
    try { (void)Json(content).dump(); }
    catch (const Json::exception&) { throw cymcp::Error("InvalidSkill", "SKILL.md must use UTF-8"); }
    SkillDocument result;
    result.markdown = std::move(content);
    const auto view = std::string_view(result.markdown);
    std::istringstream lines{std::string(view.starts_with("\xef\xbb\xbf") ? view.substr(3) : view)};
    std::string line;
    bool header_closed = false, multiline = false;
    if (std::getline(lines, line) && trim(line) == "---") while (std::getline(lines, line)) {
        if (trim(line) == "---") { header_closed = true; break; }
        if (multiline && (line.empty() || line.front() == ' ' || line.front() == '\t')) {
            if (!result.description.empty()) result.description += ' ';
            result.description += trim(line);
            continue;
        }
        multiline = false;
        if (line.starts_with("name:")) result.name = unquote(trim(line.substr(5)));
        if (line.starts_with("description:")) {
            result.description = unquote(trim(line.substr(12)));
            multiline = result.description.starts_with('|') || result.description.starts_with('>');
            if (multiline) result.description.clear();
        }
    }
    if (!header_closed || !valid_name(result.name) || result.description.empty())
        throw cymcp::Error("InvalidSkill", "SKILL.md requires a lower-case name and a description in its YAML frontmatter");
    return result;
}

std::string value(const Json& object, const char* key) {
    const auto field = object.find(key);
    return field != object.end() && field->is_string() ? field->get<std::string>() : std::string{};
}

Json structured(const Json& raw) {
    const auto direct = raw.value("structuredContent", Json());
    if (direct.is_object()) return direct;
    const auto content = raw.value("content", Json());
    if (content.is_array()) for (const auto& item : content) {
        const auto parsed = Json::parse(value(item, "text"), nullptr, false);
        if (parsed.is_object()) return parsed;
    }
    return Json::object();
}

bool exists_file(const fs::path& path) {
    std::error_code error;
    return !path_is_link(path) && fs::is_regular_file(path, error);
}
} // namespace

Json SkillDocument::to_json(bool content) const {
    Json result = {{"name", name}, {"description", description}, {"connectorId", connector}, {"path", cymcp::path_utf8(path)}};
    if (content) result["markdown"] = markdown;
    return result;
}

SkillDocument read_skill_file(const fs::path& path) {
    if (!exists_file(path)) throw cymcp::Error("NotFound", "SKILL.md cannot be read");
    std::ifstream input(path, std::ios::binary);
    std::string buffer(markdown_limit + 1, '\0');
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    if (input.bad()) throw cymcp::Error("FileError", "Cannot read SKILL.md");
    buffer.resize(static_cast<std::size_t>(input.gcount()));
    auto skill = parse_skill(std::move(buffer));
    skill.path = path;
    return skill;
}

std::string skill_markdown(std::string_view name, std::string_view description, std::string_view body) {
    return "---\nname: " + std::string(name) + "\ndescription: " + Json(description).dump() + "\n---\n\n" + std::string(body);
}

SkillDocument save_skill(const std::string& connector, const std::string& markdown, bool replace) {
    if (connector != "library" && (connector.size() != 24 || connector.find_first_not_of("0123456789abcdef") != std::string::npos))
        throw cymcp::Error("InvalidConnector", "Invalid connector identifier");
    auto record = parse_skill(markdown);
    const auto directory = library_path() / connector / record.name;
    fs::create_directories(directory);
    record.connector = connector;
    record.path = directory / "SKILL.md";
    const auto temp = directory / (cymcp::random_token(16) + ".tmp");
    {
        std::ofstream output(temp, std::ios::binary | std::ios::trunc);
        output.write(markdown.data(), static_cast<std::streamsize>(markdown.size()));
        output.close();
        if (!output) throw cymcp::Error("FileError", "Cannot write SKILL.md");
    }
    if (const auto error = publish_file(temp, record.path, replace)) {
        std::error_code ignored;
        fs::remove(temp, ignored);
        if (error == std::errc::file_exists) {
            if (read_skill_file(record.path).markdown == markdown) return record;
            throw cymcp::Error("AlreadyExists", "This connector already has a skill with that name");
        }
        throw cymcp::Error("FileError", "Cannot save SKILL.md");
    }
    return record;
}

std::vector<SkillDocument> list_skills() {
    std::vector<SkillDocument> result;
    std::error_code error;
    for (fs::directory_iterator scope(library_path(), error), end; scope != end && !error; scope.increment(error)) {
        const auto id = cymcp::path_utf8(scope->path().filename());
        std::error_code inner_error;
        for (fs::directory_iterator item(scope->path(), inner_error); item != end && !inner_error; item.increment(inner_error)) {
            try {
                auto skill = read_skill_file(item->path() / "SKILL.md");
                skill.connector = id;
                result.push_back(std::move(skill));
            } catch (const cymcp::Error&) {}
        }
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return std::tie(a.name, a.connector) < std::tie(b.name, b.connector);
    });
    return result;
}

std::vector<SkillDocument> collect_mcp_skills(const Server& server, McpActions& session, const cymcp::Cancellation& cancellation) {
    std::vector<SkillDocument> collected;
    std::set<std::string> seen;
    auto store = [&](const std::string& markdown) {
        if (markdown.empty()) return;
        try {
            auto skill = save_skill(server.id, markdown);
            if (seen.insert(skill.name).second) collected.push_back(std::move(skill));
        } catch (const cymcp::Error& error) {
            if (error.code != "InvalidSkill" && error.code != "AlreadyExists") throw;
        }
    };
    Json resources = Json::array();
    try { resources = session.resources(cancellation); }
    catch (const cymcp::Error& error) { if (error.code != "RemoteError") throw; }
    for (const auto& resource : resources) {
        cancellation.check();
        const auto uri = value(resource, "uri");
        if (uri.empty() || (!group_key(uri).ends_with("skill.md") && group_key(value(resource, "name")) != "skill.md")) continue;
        const auto response = session.read(uri, cancellation);
        const auto contents = response.value("contents", Json::array());
        for (const auto& item : contents) store(value(item, "text"));
        if (collected.size() >= 32) return collected;
    }
    const auto tools = session.tools(cancellation);
    const auto has_tool = [&](const char* name) {
        return std::any_of(tools.begin(), tools.end(), [&](const auto& tool) { return value(tool, "name") == name; });
    };
    if (!has_tool("search_skills") || !has_tool("get_skill")) return collected;
    const auto index = structured(session.call("search_skills", {{"limit", 32}}, cancellation));
    for (const auto& item : index.value("items", Json::array())) {
        const auto id = value(item, "id");
        if (id.empty()) continue;
        const auto raw = structured(session.call("get_skill", {{"skill_id", id}}, cancellation));
        auto content = value(raw, "content");
        if (content.empty()) continue;
        if (!trim(content).starts_with("---")) {
            auto name = group_key(value(raw, "name"));
            if (!valid_name(name)) name = "imported-" + cymcp::sha256(id).substr(0, 12);
            auto description = value(raw, "summary");
            if (description.empty()) description = value(raw, "title");
            content = skill_markdown(name, description, content);
        }
        store(content);
        if (collected.size() >= 32) break;
    }
    return collected;
}
} // namespace hub
