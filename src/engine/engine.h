#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <random>
#include <optional>
#include "state_view.h"
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
    std::vector<double> penalties;
    GameStateView final_state;
    std::string log_path;
};

struct RollResult {
    int roll_1;
    int roll_2;
    bool is_double;
    int total() const {
        return roll_1 + roll_2;
    };
};

class Engine {
public:
    explicit Engine(GameConfig cfg);
    GameResult run();

private:
    GameConfig cfg_;
    std::mt19937_64 rng_;
    const Board& board_;
    std::uniform_int_distribution<int> dice_;
    std::vector<AgentAdapter> agent_adapters_;
    GameStateView state_;
    std::vector<int> position_to_properties_;
    std::vector<PlayerView> players_;
    std::vector<double> penalties_;
    std::vector<PropertyView> properties_;

    std::vector<uint32_t> community_deck_;
    std::vector<uint32_t> chance_deck_;

    void init_setup();
    RollResult dice_roll();
    bool update_position(PlayerView& player, RollResult diceroll);
    void handle_position(PlayerView& player);
    int Engine::is_monopoly(const PropertyInfo* street);
    void jail(PlayerView& player);
    uint32_t get_rent(PlayerView& player);
    uint32_t get_street_rent(PlayerView& player, const PropertyInfo* street);
    uint32_t get_railroad_rent(PlayerView& player, const int railroadIndex);
    uint32_t get_utility_rent(PlayerView& player, const int utilityIndex);
    bool raise_fund(PlayerView& player, uint32_t owed);
    void pay_by_mortgage(PlayerView& player, std::vector<PropertyView*> undeveloped_assets, uint32_t amount);
    void pay_by_houses(PlayerView& player, std::vector<PropertyView*> developed_assets, uint32_t amount);
    void mortgage(PlayerView& player, PropertyView* property);
    void bankrupt(PlayerView& player, PlayerView* debtor);
    bool developed_monopoly(PropertyView* property);
    void sell_house(PlayerView& player, PropertyView* property);
    void build_house(PlayerView& player, PropertyView* property);
    void penalize(PlayerView& player);
    void community_card_draw(PlayerView& player);
    void chance_card_draw(PlayerView& player);
    bool in_jail(PlayerView& player);
    bool handle_action(PlayerView& player, Action player_action);
    void buy_property(PlayerView& player, PropertyView* property);
    void auction(PropertyView* property);
};
