#include "engine.h"
#include "board.hpp"
#include <fstream>
#include <cstdint>

Engine::Engine(GameConfig config) : cfg_(std::move(config)), rng_(cfg_.seed), dice_(1, 6) {
    // Reserve space on agent_adapters_, mildly improves performance
    agent_adapters_.reserve(cfg_.agent_specs.size());

    // Loop through specs and create the corresponding adapters
    for (const auto& spec : cfg_.agent_specs) {
        // Emplace_back calls constructor and creates the AgentAdapter
        agent_adapters_.emplace_back(spec);
    }

    for (size_t i = 0; i < agent_adapters_.size(); i++) {
        // Apparently generates a random seed
        const uint64_t seed = cfg_.seed ^ (static_cast<uint64_t>(i) + 0x9e3779b97f4a7c15ULL);
        agent_adapters_[i].game_start(i, seed);
    }

    init_setup();
}

void Engine::init_setup() {
    // Maybe should be in config.json, but not needed rn
    static constexpr uint32_t STARTING_CASH = 1500;
    static constexpr uint32_t START_POSITION = 0;
    static constexpr uint32_t NUM_PROPERTIES = 28;
    static constexpr uint32_t NUM_TILES = 40;

    const uint32_t player_count = static_cast<uint64_t>(cfg_.agent_specs.size());

    memset(&state_, 0, sizeof(state_));

    state_.game_id = cfg_.seed;
    state_.players_remaining = player_count;
    state_.num_properties = NUM_PROPERTIES;
    state_.houses_remaining = 32;
    state_.hotels_remaining = 12;
    
    players_.assign(player_count, {});
    for (uint32_t i = 0; i < player_count; i++) {
        auto& player = players_[i];
        player.player_index = i;
        player.cash = STARTING_CASH;
        player.position = 0;
        player.retired = false;

        player.in_jail = false;
        player.turns_in_jail = 0;
        player.jail_free_cards = 0;

        player.railroads_owned = 0;
        player.utilities_owned = 0;
    }

    auto& b = board();

    properties_.assign(NUM_PROPERTIES, {});
    position_to_properties_.assign(NUM_TILES, -1);
    int property_count = 0;
    for (uint32_t i = 0; i < NUM_TILES; i++) {
        const TileType tile_type = b.tiles[i].type;
        switch (tile_type) {
        case TileType::Property:
            const PropertyInfo* property = b.propertyByTile(i);
            auto& street = properties_[property_count];
            street.position = i;
            street.property_id = property->property_id;
            street.owner_index = UINT32_MAX;
            street.mortgaged = false;

            street.type = PropertyType::PROPERTY;
            street.colour_id = static_cast<uint8_t>(property->colour);
            street.house_price = property->house_cost;
            street.houses = 0;
            street.hotel = false;

            street.purchase_price = property->purchase_price;
            auto& rent_values = property->rent;
            std::tie(street.rent0, street.rent1, street.rent2, street.rent3, street.rent4, street.rentH) =
                std::make_tuple(rent_values[0], rent_values[1], rent_values[2], rent_values[3], rent_values[4], rent_values[5]);
            
            street.current_rent = 0;
            position_to_properties_[i] = property_count;
            property_count++;
            break;
        case TileType::Railroad:
            const int railroad_id = b.railroadByTile(i);
            const RailroadInfo* railroad = &b.railroads[railroad_id];
            auto& rail = properties_[property_count];
            rail.position = i;
            rail.property_id = railroad->railroad_id;
            rail.owner_index = UINT32_MAX;
            rail.mortgaged = false;

            rail.type = PropertyType::RAILROAD;

            rail.purchase_price = railroad->purchase_price;

            rail.current_rent = 0;
            position_to_properties_[i] = property_count;
            property_count++;
            break;
        case TileType::Utility:
            const int utility_id = b.utilityByTile(i);
            const UtilityInfo* utility = &b.utilities[utility_id];
            auto& util = properties_[property_count];
            util.position = i;
            util.property_id = utility->utility_id;
            util.owner_index = UINT32_MAX;
            util.mortgaged = false;

            util.type = PropertyType::UTILITY;

            util.purchase_price = utility->purchase_price;

            util.current_rent = 0;
            position_to_properties_[i] = property_count;
            property_count++;
            break;
        default:
            break;
        }
    }
}

GameResult Engine::run() {
    int turn = 0;
    while (turn < cfg_.max_turns)
    {
        turn++;
    }
}

RollResult Engine::dice_roll() {
    int roll1 = dice_(rng_);
    int roll2 = dice_(rng_);
    return {roll1, roll2, roll1 == roll2};
}