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

std::string to_json(const PlayerView& p) {
    std::ostringstream oss;
    oss << '{';

    oss << "\"player_index\":"    << p.player_index    << ',';
    oss << "\"cash\":"            << p.cash            << ',';
    oss << "\"position\":"        << p.position        << ',';
    oss << "\"retired\":"         << (p.retired ? "true" : "false") << ',';

    oss << "\"in_jail\":"         << (p.in_jail ? "true" : "false") << ',';
    oss << "\"turns_in_jail\":"   << p.turns_in_jail   << ',';
    oss << "\"jail_free_cards\":" << p.jail_free_cards << ',';
    oss << "\"double_rolls\":"    << p.double_rolls    << ',';

    oss << "\"railroads_owned\":" << static_cast<unsigned int>(p.railroads_owned) << ',';
    oss << "\"utilities_owned\":" << static_cast<unsigned int>(p.utilities_owned);

    oss << '}';
    return oss.str();
}

std::string to_json(const PropertyView& pr) {
    std::ostringstream oss;
    oss << '{';

    oss << "\"position\":"      << pr.position      << ',';
    oss << "\"property_id\":"   << pr.property_id   << ',';
    oss << "\"owner_index\":"   << pr.owner_index   << ',';
    oss << "\"is_owned\":"      << (pr.is_owned ? "true" : "false") << ',';
    oss << "\"mortgaged\":"     << (pr.mortgaged ? "true" : "false") << ',';

    // PropertyType as integer
    oss << "\"type\":"          << static_cast<int>(pr.type) << ',';

    oss << "\"colour_id\":"     << static_cast<unsigned int>(pr.colour_id) << ',';
    oss << "\"house_price\":"   << pr.house_price   << ',';
    oss << "\"houses\":"        << static_cast<unsigned int>(pr.houses) << ',';
    oss << "\"hotel\":"         << (pr.hotel ? "true" : "false") << ',';

    oss << "\"purchase_price\":"<< pr.purchase_price<< ',';
    oss << "\"rent0\":"         << pr.rent0         << ',';
    oss << "\"rent1\":"         << pr.rent1         << ',';
    oss << "\"rent2\":"         << pr.rent2         << ',';
    oss << "\"rent3\":"         << pr.rent3         << ',';
    oss << "\"rent4\":"         << pr.rent4         << ',';
    oss << "\"rentH\":"         << pr.rentH         << ',';

    oss << "\"current_rent\":"  << pr.current_rent;

    oss << "\"is_monopoly\":"  << pr.is_monopoly;

    oss << '}';
    return oss.str();
}

std::string to_json(const GameStateView& s) {
    std::ostringstream oss;
    oss << '{';

    oss << "\"game_id\":"            << s.game_id            << ',';
    oss << "\"houses_remaining\":"   << s.houses_remaining   << ',';
    oss << "\"hotels_remaining\":"   << s.hotels_remaining   << ',';
    oss << "\"current_player_index\":" << s.current_player_index << ',';
    oss << "\"owed\":"               << s.owed               << ',';

    // players array
    oss << "\"players\":[";
    if (s.players && s.players_remaining > 0) {
        for (uint32_t i = 0; i < s.players_remaining; ++i) {
            if (i > 0) oss << ',';
            oss << to_json(s.players[i]);
        }
    }
    oss << "],";

    // properties array
    oss << "\"properties\":[";
    if (s.properties && s.num_properties > 0) {
        for (uint32_t i = 0; i < s.num_properties; ++i) {
            if (i > 0) oss << ',';
            oss << to_json(s.properties[i]);
        }
    }
    oss << "],";

    // include counts explicitly as well
    oss << "\"players_remaining\":" << s.players_remaining << ',';
    oss << "\"num_properties\":"    << s.num_properties;

    oss << '}';
    return oss.str();
}

std::string to_json(const GameResult& r) {
    std::ostringstream oss;
    oss << '{';

    oss << "\"game_id\":" << r.game_id << ',';
    oss << "\"turns\":"   << r.turns   << ',';
    oss << "\"winner\":"  << r.winner  << ',';

    oss << "\"penalties\":[";
    for (std::size_t i = 0; i < r.penalties.size(); ++i) {
        if (i > 0) oss << ',';
        oss << r.penalties[i];
    }
    oss << "],";

    oss << "\"player_scores\":[";
    for (std::size_t i = 0; i < r.player_scores.size(); ++i) {
        if (i > 0) oss << ',';
        oss << r.player_scores[i];
    }
    oss << "],";

    oss << "\"final_state\":" << to_json(r.final_state) << ',';

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
    std::cerr << "Engine.run() complete\n";
    std::cerr << "Players pointer: " << result.final_state.players << "\n";
    std::cerr << "Players count: " << result.final_state.players_remaining << "\n";
    std::cerr << "Properties pointer: " << result.final_state.properties << "\n";
    std::cerr << "Properties count: " << result.final_state.num_properties << "\n";

    std::cout << to_json(result) << '\n';
    return 0;
}
