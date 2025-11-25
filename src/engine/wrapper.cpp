#include "engine.h"
#include "agent_adapter.h"
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <iomanip>
#include <sstream>

// Escape a string for safe JSON output
std::string json_escape(const std::string& in) {
    std::ostringstream oss;
    oss << '"';
    for (unsigned char c : in) {
        switch (c) {
        case '\\': oss << "\\\\"; break;
        case '"':  oss << "\\\""; break;
        case '\b': oss << "\\b";  break;
        case '\f': oss << "\\f";  break;
        case '\n': oss << "\\n";  break;
        case '\r': oss << "\\r";  break;
        case '\t': oss << "\\t";  break;
        default:
            if (c < 0x20) {
                oss << "\\u"
                    << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<int>(c);
            } else {
                oss << c;
            }
        }
    }
    oss << '"';
    return oss.str();
}

// You need *some* way to serialize GameStateView.
// Adjust this to whatever your API looks like.
std::string to_json(const GameStateView& s) {
    // Example placeholder:
    // return s.to_json();  // if you already have this
    // or manually build JSON for the state here.
    return "{}"; // TODO: implement actual serialization
}

std::string to_json(const GameResult& r) {
    std::ostringstream oss;
    oss << '{';

    oss << "\"game_id\":" << r.game_id << ',';
    oss << "\"turns\":"   << r.turns   << ',';
    oss << "\"winner\":"  << r.winner  << ',';

    // penalties array
    oss << "\"penalties\":[";
    for (std::size_t i = 0; i < r.penalties.size(); ++i) {
        if (i > 0) oss << ',';
        oss << r.penalties[i];
    }
    oss << "],";

    // final_state as nested JSON
    oss << "\"final_state\":" << to_json(r.final_state) << ',';

    // log_path as string
    oss << "\"log_path\":" << json_escape(r.log_path);

    oss << '}';
    return oss.str();
}

[[noreturn]] void usage(const char* prog) {
    std::cerr << "Usage:\n"
              << "  " << prog
              << " <game_id> <seed> <turns>"
                 " --agent <path> <config_file> <name> [--agent ...]\n";
    std::exit(EXIT_FAILURE);
}

uint64_t parse_u64(const char* s, const char* what) {
    try {
        std::size_t idx = 0;
        uint64_t val = std::stoull(s, &idx, 10);
        if (s[idx] != '\0') throw std::invalid_argument("trailing chars");
        return val;
    } catch (...) {
        std::cerr << "Invalid " << what << ": " << s << "\n";
        std::exit(EXIT_FAILURE);
    }
}

std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "Failed to open config file: " << path << "\n";
        std::exit(EXIT_FAILURE);
    }
    std::stringstream buffer;
    buffer << f.rdbuf();
    return buffer.str();
}

int main(int argc, char* argv[]) {
    if (argc < 4) usage(argv[0]);

    uint64_t game_id = parse_u64(argv[1], "game_id");
    uint64_t seed    = parse_u64(argv[2], "seed");
    uint32_t turns   = static_cast<uint32_t>(parse_u64(argv[3], "turns"));

    std::vector<AgentSpec> agent_specs;

    int i = 4;
    while (i < argc) {
        std::string_view arg = argv[i];

        if (arg == "--agent") {
            if (i + 3 >= argc) {
                std::cerr << "--agent requires: <path> <config_file> <name>\n";
                usage(argv[0]);
            }

            std::string path          = argv[i + 1];
            std::string config_file   = argv[i + 2];
            std::string name          = argv[i + 3];

            AgentSpec spec{
                path,
                read_file(config_file), // load JSON content
                name
            };

            agent_specs.push_back(std::move(spec));
            i += 4;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            usage(argv[0]);
        }
    }

    if (agent_specs.empty()) {
        std::cerr << "Need at least one --agent.\n";
        usage(argv[0]);
    }

    GameConfig config = {
        game_id,
        seed,
        turns,
        agent_specs,
    };

    Engine engine(config);
    GameResult result = engine.run();

    std::cout << to_json(result) << '\n';
    return 0;
}
