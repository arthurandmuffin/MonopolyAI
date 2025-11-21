#include "engine.h"
#include "board.hpp"
#include <fstream>
#include <cstdint>
#include <cassert>

Engine::Engine(GameConfig config) : cfg_(std::move(config)), rng_(cfg_.seed), dice_(1, 6), board_(board()) {
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
    // To get player info with player_index i, its players_[i]
    // Initialize player state
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
        player.double_rolls = 0;

        player.railroads_owned = 0;
        player.utilities_owned = 0;
    }

    auto& b = this->board_;

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
            street.is_owned = false;
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
            rail.is_owned = false;
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
            util.is_owned = false;
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
    this->state_.players = this->players_.data();
    this->state_.properties = this->properties_.data();
}

GameResult Engine::run() {
    int turn = 0;
    while (turn < cfg_.max_turns)
    {
        for (auto it = this->players_.begin(); it != this->players_.end(); it++) {
            auto& player = *it;
            if (player.retired) {
                continue;
            }

            uint32_t index = player.player_index;
            RollResult dice_roll = this->dice_roll();
            bool in_jail = update_position(player, dice_roll);
            if (in_jail) {
                continue;
            }

            uint32_t rent = get_rent(player);

            while (true) {

                // TODO: 
                // implement pay_rent function
                // take action 

                AgentAdapter& agent = this->agent_adapters_[index];
                Action agent_action = agent.agent_turn(&this->state_);
            }
        }
        turn++;
    }
}

bool Engine::update_position(PlayerView& player, RollResult diceroll) {
    if (diceroll.is_double) {
        if (player.double_rolls >= 2) {
            jail(player);
            return true;
        } else {
            player.double_rolls += 1;
        }
    } else {
        player.double_rolls = 0;
    }

    uint8_t new_position = player.position + diceroll.roll_1 + diceroll.roll_2;
    uint8_t num_tiles = this->board_.tiles.size();
    if (new_position >= num_tiles) {
        // Passed Go
        player.cash += 200; // config
        new_position %= num_tiles;
    }
    player.position = new_position;

    return false;
}

void Engine::handle_position(PlayerView& player) {
    switch (this->board_.tiles[player.position].type) {
    case (TileType::Property):
    case (TileType::Railroad):
    case (TileType::Utility):
        uint32_t rent = get_rent(player);
    case (TileType::Tax):
        uint32_t cost = 200; //config, implement 10% in future
    case (TileType::Chance):
        // Todo: Change
    case (TileType::Community):
        // Todo: Community
    }
}

uint32_t Engine::get_rent(PlayerView& player) {
    // Get rent using board, assume houses are legal
    // implement functions get_street_rent, get_railroad_rent, get_utility_rent
    const PropertyInfo* street = this->board_.propertyByTile(player.position);
    const int railroadIndex = this->board_.railroadByTile(player.position);
    const int utilityIndex = this->board_.utilityByTile(player.position);
    if (street) {
        return Engine::get_street_rent(player, street);
    } else if (railroadIndex != -1) {
        return Engine::get_railroad_rent(player, railroadIndex);
    } else if (utilityIndex != -1) {
        return Engine::get_utility_rent(player, utilityIndex);
    }
}

uint32_t Engine::get_street_rent(PlayerView& player, const PropertyInfo* street) {
    int index = this->position_to_properties_[player.position];
    PropertyView& property = this->properties_[index];
    assert(property.type == PropertyType::PROPERTY);

    if (property.mortgaged || !property.is_owned || property.owner_index == player.player_index) {
        return 0;
    }

    // Nothing built
    if (!property.hotel && property.houses == 0) {
        uint32_t base_rent = street->rent[0];
        if (is_monopoly(street)) {
            return 2 * base_rent;
        }
        return base_rent;
    } else if (!property.hotel) {
        assert(property.houses <= 4);
        return street->rent[property.houses]; // check indexing
    } else {
        // Must be a hotel
        return street->rent[-1];
    }
}

bool Engine::is_monopoly(const PropertyInfo* street) {
    const ColourGroup& colour_group = this->board_.tilesOfColour(street->colour);
    uint32_t owner = -1;
    for (int i = 0; i < colour_group.count; i++) {
        PropertyView& property = this->properties_[this->position_to_properties_[colour_group.tiles[i]]];
        // no monopoly if unowned property
        if (!property.is_owned) {
            return false;
        }
        // init
        if (owner == -1) {
            owner = property.owner_index;
        } else if (owner != property.owner_index) {
            return false;
        }
    }
    return true;
}

uint32_t Engine::get_railroad_rent(PlayerView& player, const int railroadIndex) {
    int index = this->position_to_properties_[player.position];
    PropertyView& railroad = this->properties_[index];
    assert(railroad.type == PropertyType::RAILROAD);

    if (!railroad.is_owned || railroad.mortgaged || railroad.owner_index == player.player_index) {
        return 0;
    }
    return railroad.current_rent;
}

uint32_t Engine::get_utility_rent(PlayerView& player, const int utilityIndex) {
    int index = this->position_to_properties_[player.position];
    PropertyView& utility = this->properties_[index];
    assert(utility.type == PropertyType::UTILITY);

    if (!utility.is_owned || utility.mortgaged || utility.owner_index == player.player_index) {
        return 0;
    }

    RollResult roll = this->dice_roll();
    PlayerView& utility_owner = players_[utility.owner_index];
    assert(utility_owner.utilities_owned <= 2);
    if (utility_owner.utilities_owned == 1) {
        return 4 * (roll.roll_1 + roll.roll_2);
    } else {
        return 10 * (roll.roll_1 + roll.roll_2);
    }
}

bool Engine::pay(PlayerView& player, uint32_t amount) {
    
}

// pay_rent:
// Use internal heuristic:
// 1. Cash
// 2. Start with single colour, utilities, and railroads
// 3. Monopolies w/o houses, then houses

void Engine::jail(PlayerView& player) {
    player.in_jail = true;
    player.turns_in_jail = 0;
    player.position = this->board_.jailPosition();
    return;
}

RollResult Engine::dice_roll() {
    int roll1 = dice_(rng_);
    int roll2 = dice_(rng_);
    return {roll1, roll2, roll1 == roll2};
}