#include "hub_service.hpp"
#include "platform.hpp"

#include <cymcp/mcp.hpp>
#include <iostream>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

using hub::Json;

static int run(const std::vector<std::string>& arguments) {
    try {
        const auto args = hub::parse_arguments(arguments);
        if (args.help) {
            std::cout << "CyMCPHubMcp [--workspace PATH] [--config FILE] [--no-port-scan] [--list]\n"
                         "Runs an MCP stdio server. With --list, prints a JSON inventory and exits.\n";
        } else if (args.list) {
            const auto snapshot = hub::scan(args.options);
            auto servers = Json::array();
            for (const auto& server : snapshot.servers) servers.push_back(server.to_json(true));
            std::cout << Json({{"servers", servers}, {"inventory", hub::inventory_metadata(snapshot, args.options.scan_ports)}}).dump(2) << '\n';
        } else {
            const auto tools = hub::make_hub_registry(args.options, std::make_shared<hub::Journal>());
#ifdef _WIN32
            _setmode(_fileno(stdin), _O_BINARY);
            _setmode(_fileno(stdout), _O_BINARY);
#endif
            cymcp::McpServer(*tools, "CyMCPHub").serve_stdio(std::cin, std::cout);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << cymcp::redact_error_message(error.what()) << '\n';
        return 1;
    }
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    std::vector<std::string> arguments;
    for (int i = 0; i < argc; ++i) arguments.push_back(cymcp::path_utf8(hub::fs::path(argv[i])));
    return run(arguments);
}
#else
int main(int argc, char** argv) { return run({argv, argv + argc}); }
#endif
