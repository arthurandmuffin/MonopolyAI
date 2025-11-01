#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <random>
#include <optional>
#include "agent_abi.h"
#include "agent_adapter.h"

struct GameConfig {
  uint64_t game_id;
  uint64_t seed;
  uint32_t max_turns;
  std::vector<AgentSpec> agent_specs;
};

struct GameResult {
    uint64_t game_id;
    uint64_t turns;
    int winner; // -1 if draw/timeout
    std::vector<int32_t> final_cash;
    std::string log_path;
};

class Engine {
public:
    explicit Engine(GameConfig cfg);
    GameResult run();

private:
    GameConfig cfg_;
    std::mt19937_64 rng_;
    GameStateView state_;
    std::vector<AgentAdapter> agent_adapters_;

    void init_setup();

    void auction(uint32_t property_id);
};
