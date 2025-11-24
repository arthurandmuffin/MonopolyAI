#include "engine.h"
#include "board.hpp"
#include <fstream>
#include <cstdint>
#include <set>
#include <cassert>
#include <algorithm>

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
        penalties_[i] = 0;
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

    this->community_deck_.resize(16);
    std::iota(this->community_deck_.begin(), this->community_deck_.end(), 0);
    std::shuffle(this->community_deck_.begin(), this->community_deck_.end(), this->rng_);

    this->chance_deck_.resize(16);
    std::iota(this->chance_deck_.begin(), this->chance_deck_.end(), 0);
    std::shuffle(this->chance_deck_.begin(), this->chance_deck_.end(), this->rng_);
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
    case (TileType::Chance):
        this->chance_card_draw(player);
        if (player.retired) {
            return;
        }
    case (TileType::Community):
        this->community_card_draw(player);
        if (player.retired) {
            return;
        }
    }

    uint32_t rent = 0;
    uint32_t cost = 0;
    PlayerView* debtor = nullptr;
    switch (this->board_.tiles[player.position].type) {
    case (TileType::Property):
    case (TileType::Railroad):
    case (TileType::Utility):
        int index = this->position_to_properties_[player.position];
        PropertyView& property = this->properties_[index];
        debtor = &this->players_[property.owner_index];
        rent = get_rent(player);
        break;
    case (TileType::Tax):
        cost = 200; //config, implement 10% in future
        break;
    case (TileType::GoToJail):
        this->jail(player);
        break;
    }

    assert(rent == 0 || cost == 0);

    if (rent > 0) {
        bool payable = this->raise_fund(player, rent);
        if (payable) {
            assert(player.cash >= rent);
            player.cash -= rent;
            debtor->cash += rent;
        } else {
            this->bankrupt(player, debtor);
        }
    }

    if (cost > 0) {
        bool payable = this->raise_fund(player, rent);
        if (payable) {
            assert(player.cash >= cost);
            player.cash -= cost;
        } else {
            this->bankrupt(player, nullptr);
        }
    }
}

void Engine::community_card_draw(PlayerView& player) {
    uint32_t drawn_card = this->community_deck_[0];
    this->community_deck_.erase(this->community_deck_.begin());
    assert(drawn_card < 16);

    switch (drawn_card)
    {
    case 0:
        // Advance to Go & collect $200.
        player.position = 0;
        player.cash += 200;
        break;
    case 1:
        // Bank error in your favor. Collect $200.
        player.cash += 200;
        break;
    case 2:
        // Doctor's fee. Pay $50.
        bool payable = this->raise_fund(player, 50);
        if (!payable) {
            this->bankrupt(player, nullptr);
        } else {
            assert(player.cash >= 50);
            player.cash -= 50;
        }
        break;
    case 3:
        // From sale of stock you get $50
        player.cash += 50;
        break;
    case 4:
        // Get out of jail free card
        player.jail_free_cards += 1;
        return;
    case 5:
        // Go to jail
        this->jail(player);
        break;
    case 6:
        // Grand Opera Night, colelct $50 from each player
        for (auto& other_player : this->players_) {
            if (player.player_index == other_player.player_index || other_player.retired) {
                continue;
            }
            bool payable = this->raise_fund(other_player, 50);
            if (!payable) {
                this->bankrupt(other_player, &player);
            } else {
                assert(other_player.cash >= 50);
                other_player.cash -= 50;
                player.cash += 50;
            }
        }
        break;
    case 7:
        // Holiday Christmas Fund matures, collect $100
        player.cash += 100;
        break;
    case 8:
        // Income tax refund, collect $20
        player.cash += 20;
        break;
    case 9:
        // Birthday, collect $10 from each player
        for (auto& other_player : this->players_) {
            if (player.player_index == other_player.player_index || other_player.retired) {
                continue;
            }
            bool payable = this->raise_fund(other_player, 10);
            if (!payable) {
                this->bankrupt(other_player, &player);
            } else {
                assert(other_player.cash >= 10);
                other_player.cash -= 50;
                player.cash += 50;
            }
        }
        break;
    case 10:
        // Life insurance matures, collect $100
        player.cash += 100;
        break;
    case 11:
        // Hospital fees, pay $50
        bool payable = this->raise_fund(player, 50);
        if (!payable) {
            this->bankrupt(player, nullptr);
        } else {
            assert(player.cash >= 50);
            player.cash -= 50;
        }
        break;
    case 12:
        // School fees, pay $50
        bool payable = this->raise_fund(player, 50);
        if (!payable) {
            this->bankrupt(player, nullptr);
        } else {
            assert(player.cash >= 50);
            player.cash -= 50;
        }
        break;
    case 13:
        // Receive consultancy fee of $25
        player.cash += 25;
        break;
    case 14:
        // Street repairs, pay $40 per house and $115 per hotel
        uint32_t repair_cost = 0;
        for (auto& property : this->properties_) {
            if (property.owner_index != player.player_index || property.type != PropertyType::PROPERTY) {
                continue;
            }
            if (property.hotel) {
                repair_cost += 115;
            } else {
                repair_cost += 40 * property.houses;
            }
        }

        bool payable = this->raise_fund(player, repair_cost);
        if (!payable) {
            this->bankrupt(player, nullptr);
        } else {
            assert(player.cash >= repair_cost);
            player.cash -= repair_cost;
        }
        break;
    case 15:
        // 2nd place in beauty contest, colelct $10
        player.cash += 10;
        break;
    }
    this->community_deck_.push_back(drawn_card);
}

void Engine::bankrupt(PlayerView& player, PlayerView* debtor) {
    if (debtor) {
        debtor->cash += player.cash;

        player.cash = 0;
        player.retired = true;
        player.in_jail = false;
        player.turns_in_jail = 0;
        player.jail_free_cards = 0;
        player.double_rolls = 0;
        player.railroads_owned = 0;
        player.utilities_owned = 0;
    }

    std::vector<PropertyView*> assets{};
    for (auto& property : this->properties_) {
        if (property.owner_index == player.player_index) {
            assets.push_back(&property);
        }
    }

    if (debtor) {
        for (auto* asset : assets) {
            asset->owner_index = debtor->player_index;
        }
    } else {
        for (auto* asset: assets) {
            this->auction(asset);
        }
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

int Engine::is_monopoly(const PropertyInfo* street) {
    const ColourGroup& colour_group = this->board_.tilesOfColour(street->colour);
    uint32_t owner = -1;
    for (int i = 0; i < colour_group.count; i++) {
        PropertyView& property = this->properties_[this->position_to_properties_[colour_group.tiles[i]]];
        // no monopoly if unowned property
        if (!property.is_owned) {
            return 0;
        }
        // init
        if (owner == -1) {
            owner = property.owner_index;
        } else if (owner != property.owner_index) {
            return 0;
        }
    }
    return static_cast<int>(colour_group.count);
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

bool Engine::raise_fund(PlayerView& player, uint32_t owed) {
    // Pay with cash first
    if (player.cash >= owed) {
        return true;
    }

    // Handle only non-mortgaged assets w/ no development
    std::vector<PropertyView*> undeveloped_assets;
    for (auto &p : this->properties_) {
        if (p.owner_index == player.player_index && !p.hotel && p.houses == 0 && !p.mortgaged) {
            undeveloped_assets.push_back(&p);
        }
    }

    this->pay_by_mortgage(player, undeveloped_assets, owed);
    if (player.cash >= owed) {
        return true;
    }

    std::vector<PropertyView*> developed_assets;
    for (auto &p : this->properties_) {
        if (p.owner_index == player.player_index && p.type == PropertyType::PROPERTY) {
            if (p.houses > 0 || p.hotel) {
                developed_assets.push_back(&p);
            }
        }
    }
    // loop through pay by mortgage whenever monopoly frees up???
    this->pay_by_houses(player, developed_assets, owed);
    if (player.cash >= owed) {
        return true;
    }
    return false;
}

// Future: probably split each property types impact eval into separate fns
void Engine::pay_by_mortgage(PlayerView& player, std::vector<PropertyView*> undeveloped_assets, uint32_t amount) {
    // assumption: undeveloped assets are railroads / utilities / properties w/ no houses + not mortgaged
    while (amount > player.cash) {
        PropertyView* lowest_impact_asset = nullptr;
        double lowest_impact = std::numeric_limits<double>::infinity();

        for (auto it = undeveloped_assets.begin(); it != undeveloped_assets.end();) {
            PropertyView* asset = *it;
            if (asset->mortgaged) {
                it = undeveloped_assets.erase(it);
                continue;
            }

            double impact = 0;
            switch (asset->type) {
            case (PropertyType::PROPERTY): {
                const PropertyInfo* asset_info = this->board_.propertyByTile(asset->position);
                assert(asset_info);

                if (is_monopoly(asset_info)) {
                    const ColourGroup group = this->board_.tilesOfColour(asset_info->colour);
                    bool mortgage_ineligible = false;

                    for (uint8_t j = 0; j < group.count; j++) {
                        int index = this->position_to_properties_[group.tiles[j]];
                        if (this->properties_[index].houses > 0) {
                            mortgage_ineligible = true;
                            break;
                        } else {
                            if (group.tiles[j] != asset->position) {
                                // Affected monopolies
                                const PropertyInfo& affected_asset_info = *this->board_.propertyByTile(group.tiles[j]);
                                impact += this->board_.tile_probability[group.tiles[j]] 
                                    * (static_cast<double>(affected_asset_info.rent[0]));
                            } else {
                                // Actual mortgaged property
                                assert(group.tiles[j] == asset->position);
                                impact += this->board_.tile_probability[group.tiles[j]] * asset->current_rent;
                            }
                        }
                    }

                    if (mortgage_ineligible) {
                        it = undeveloped_assets.erase(it);
                        continue;
                    }
                } else {
                    impact = this->board_.tile_probability[asset->position] * asset->current_rent;
                }
                break;
            }
            case (PropertyType::RAILROAD): {
                const auto& rent_info = this->board_.railroads[0].rent;
                int railroads_active = 0;
                for (auto position : this->board_.railroad_positions) {
                    int index = this->position_to_properties_[position];
                    const PropertyView& railroad = this->properties_[index];
                    if (railroad.owner_index == player.player_index && !railroad.mortgaged) {
                        railroads_active++;
                    }
                }
                assert(railroads_active >= 1);

                int current_rent = static_cast<int>(asset->current_rent);
                // new rent for remaining non-mortgaged railroads
                int new_rent = (railroads_active > 1) ? rent_info[railroads_active - 2] : 0;

                for (auto position : this->board_.railroad_positions) {
                    int index = this->position_to_properties_[position];
                    PropertyView& railroad = this->properties_[index];
                    if (railroad.owner_index != player.player_index || railroad.mortgaged) {
                        continue;
                    }

                    double prob = this->board_.tile_probability[railroad.position];

                    if (railroad.property_id == asset->property_id) {
                        // mortgaged one: rent -> 0
                        impact += prob * current_rent;
                    } else {
                        // others: current_rent -> new_rent
                        impact += prob * (current_rent - new_rent);
                    }
                }
                break;
            }
            case (PropertyType::UTILITY): {
                const auto& multipliers = this->board_.utilities[0].multiplier;
                int utilities_active = 0;
                for (auto position : this->board_.utility_positions) {
                    int index = this->position_to_properties_[position];
                    const PropertyView& utility = this->properties_[index];
                    if (utility.owner_index == player.player_index && !utility.mortgaged) {
                        utilities_active++;
                    }
                }
                assert(utilities_active >= 1);

                const double average_roll = 7.0;
                int current_multiplier = multipliers[utilities_active - 1];
                int new_multiplier = (utilities_active > 1) ? multipliers[utilities_active - 2] : 0;

                for (auto position : this->board_.utility_positions) {
                    int index = this->position_to_properties_[position];
                    const PropertyView& utility = this->properties_[index];
                    if (utility.owner_index != player.player_index || utility.mortgaged) {
                        continue;
                    }

                    double prob = this->board_.tile_probability[position];

                    if (utility.property_id == asset->property_id) {
                        impact += prob * current_multiplier * average_roll;
                    } else {
                        impact += prob * (current_multiplier - new_multiplier) * average_roll;
                    }
                }
                break;
            }}
            if (impact < lowest_impact) {
                lowest_impact = impact;
                lowest_impact_asset = asset;
            }
            it++;
        }
        // has found least impactful asset (if any)
        if (lowest_impact_asset) {
            this->mortgage(player, lowest_impact_asset);
            if (player.cash >= amount) {
                return;
            }
        } else {
            return;
        }
    }
    return;
}

void Engine::pay_by_houses(PlayerView& player, std::vector<PropertyView*> developed_assets, uint32_t amount) {
    std::set<Colour> monopolies;
    for (auto* asset : developed_assets) {
        monopolies.insert(static_cast<Colour>(asset->colour_id));
    }

    while (amount > player.cash) {
        std::unordered_map<Colour, uint8_t> max_houses;
        for (auto colour : monopolies) {
            const ColourGroup& group = this->board_.tilesOfColour(colour);
            for (int i = 0; i < group.count; i++) {
                int index = this->position_to_properties_[group.tiles[i]];
                PropertyView* property = &this->properties_[index];
                assert(property->type == PropertyType::PROPERTY);
                assert(property->owner_index == player.player_index);
                max_houses[group.colour] = std::max(max_houses[group.colour], property->houses);
                if (property->hotel) {
                    max_houses[group.colour] = 5;
                }
            }
        }

        PropertyView* lowest_impact_asset = nullptr;
        double lowest_impact = std::numeric_limits<double>::infinity();
        int lowest_impact_house_count = 0;

        for (auto* asset : developed_assets) {
            if (asset->houses == 0) {
                continue;
            }
            int house_count = max_houses[static_cast<Colour>(asset->colour_id)];
            if (house_count > asset->houses || (house_count == 5 && !asset->hotel)) {
                continue;
            }
            assert(asset->houses > 0);

            double tile_probability = this->board_.tile_probability[asset->position];
            double money_raised = 0;
            double impact = 0;

            if (asset->hotel && this->state_.houses_remaining < 4) {
                int houses_available = this->state_.houses_remaining;
                auto it = monopolies.find(static_cast<Colour>(asset->colour_id));
                assert(it != monopolies.end());
                Colour colour = *it;
                const ColourGroup& group = this->board_.tilesOfColour(colour);
                for (int i = 0; i < group.count; i++) {
                    int index = this->position_to_properties_[group.tiles[i]];
                    PropertyView& property = this->properties_[index];
                    if (!property.hotel) {
                        assert(property.houses == 4);
                        houses_available += property.houses;
                    }
                }

                int max_houses = (houses_available + group.count - 1) / group.count; // ceil-div
                double total_raised = 0;
                double total_rent_lost = 0;

                for (int i = group.count - 1; i >= 0; i--) {
                    // number of properties left = i+1
                    int num_houses = max_houses;
                    if (houses_available < (i + 1) * max_houses) {
                        num_houses = max_houses - 1;
                    }
                    int index = this->position_to_properties_[group.tiles[i]];
                    PropertyView& asset = this->properties_[index];
                    const PropertyInfo& asset_info = *this->board_.propertyByTile(group.tiles[i]);
                    double rent_diff = asset_info.rent[asset.houses] - asset_info.rent[num_houses];
                    double income = static_cast<double>((asset.houses - num_houses) * asset_info.house_cost) / 2;
                    money_raised += income;
                    
                    total_rent_lost += this->board_.tile_probability[group.tiles[i]] * rent_diff;
                    total_raised += income;
                    houses_available -= num_houses;
                }
                impact = total_rent_lost / total_raised;
            } else {
                const PropertyInfo& asset_info = *this->board_.propertyByTile(asset->position);
                assert(asset->current_rent == asset_info.rent[asset->houses]);
                double rent_diff = asset_info.rent[asset->houses] - asset_info.rent[asset->houses - 1];
                money_raised = static_cast<double>(asset->house_price) / 2;
                impact = tile_probability * rent_diff / money_raised;
            }

            if (impact < lowest_impact) {
                lowest_impact = impact;
                lowest_impact_asset = asset;
            }
        }
        
        if (lowest_impact_asset) {
            this->sell_house(player, lowest_impact_asset);
            if (player.cash >= amount) {
                return;
            }
        } else {
            return;
        }

        if (!developed_monopoly(lowest_impact_asset)) {
            monopolies.erase(static_cast<Colour>(lowest_impact_asset->colour_id));

            ColourGroup group = this->board_.tilesOfColour(static_cast<Colour>(lowest_impact_asset->colour_id));
            std::vector<PropertyView*> newly_undeveloped_assets;
            for (int i = 0; i < group.count; i++) {
                int index = this->position_to_properties_[group.tiles[i]];
                PropertyView* p = &this->properties_[index];
                assert(p->houses == 0);
                newly_undeveloped_assets.push_back(p);
            }
            this->pay_by_mortgage(player, newly_undeveloped_assets, amount);
            if (player.cash >= amount) {
                return;
            }
        }
    }
    return;
}

// check legality in action handling, including handling even building
void Engine::build_house(PlayerView& player, PropertyView* property) {
    assert(!property->hotel);
    assert(player.cash >= property->house_price);

    const PropertyInfo* property_info = this->board_.propertyByTile(property->position);
    if (property->houses < 4) {
        assert(this->state_.houses_remaining > 0);
        player.cash -= property->house_price;
        this->state_.houses_remaining--;
        property->houses++;
        property->current_rent = property_info->rent[property->houses];
    } else {
        assert(property->houses == 4);
        assert(this->state_.hotels_remaining > 0);
        player.cash -= property->house_price;
        this->state_.hotels_remaining--;
        this->state_.houses_remaining += 4;
        property->houses++;
        property->hotel = true;
        property->current_rent = property_info->rent[property->houses];
    }
}

void Engine::sell_house(PlayerView& player, PropertyView* property) {
    assert(property->owner_index == player.player_index);
    assert(property->houses > 0);

    const PropertyInfo* property_info = this->board_.propertyByTile(property->position);
    if (!property->hotel) {
        property->houses--;
        property->current_rent = property_info->rent[property->houses];
        this->state_.houses_remaining++;
        player.cash += property->house_price / 2;
        return;
    }
    assert(property->houses == 5);

    if (this->state_.houses_remaining >= 4) {
        property->hotel = false;
        property->houses--;
        property->current_rent = property_info->rent[property->houses];
        this->state_.hotels_remaining++;
        this->state_.houses_remaining -= 4;
        player.cash += property->house_price / 2;
        return;
    }

    int houses_pool = this->state_.houses_remaining;
    const ColourGroup group = this->board_.tilesOfColour(property_info->colour);
    for (int i = 0; i < group.count; i++) {
        int index = this->position_to_properties_[group.tiles[i]];
        PropertyView* p = &this->properties_[index];
        if (!p->hotel) {
            houses_pool += p->houses;
        }
    }

    int max_houses = (houses_pool + group.count - 1) / group.count;

    for (int i = group.count - 1; i >= 0; i--) {
        int num_houses = max_houses;
        if (houses_pool < (i + 1) * max_houses) {
            num_houses = max_houses - 1;
        }
        int index = this->position_to_properties_[group.tiles[i]];
        PropertyView* p = &this->properties_[index];
        const PropertyInfo* p_info = this->board_.propertyByTile(p->position);

        int cur_houses = p->houses;
        p->houses = num_houses;
        p->current_rent = p_info->rent[num_houses];
        p->hotel = false;
        if (cur_houses == 5) {
            this->state_.hotels_remaining++;
            houses_pool -= num_houses;
        } else {
            this->state_.houses_remaining += cur_houses - num_houses;
            houses_pool -= num_houses;
        }
        player.cash += p_info->house_cost * (cur_houses - num_houses) / 2;
    }
    this->state_.houses_remaining = houses_pool;
    return;
}

bool Engine::developed_monopoly(PropertyView* property) {
    if (property->houses > 0) {
        return true;
    }
    const ColourGroup& group = this->board_.tilesOfColour(static_cast<Colour>(property->colour_id));
    for (int i = 0; i < group.count; i++) {
        int index = this->position_to_properties_[group.tiles[i]];
        PropertyView* p = &this->properties_[index];
        if (p->houses > 0) {
            return true;
        }
    }
    return false;
}

void Engine::mortgage(PlayerView& player, PropertyView* property) {
    assert(!property->mortgaged);
    assert(property->owner_index == player.player_index);
    switch (property->type)
    {
    case (PropertyType::PROPERTY): {
        assert(!property->hotel);
        assert(property->houses == 0);
        const PropertyInfo* asset_info = this->board_.propertyByTile(property->position);
        if (is_monopoly(asset_info)) {
            const ColourGroup group = this->board_.tilesOfColour(asset_info->colour);
            for (int i = 0; i < group.count; i++) {
                int index = this->position_to_properties_[group.tiles[i]];
                PropertyView& p = this->properties_[index];
                assert(p.owner_index == player.player_index);
                if (p.property_id == property->property_id) {
                    // property to be mortgaged
                    p.mortgaged = true;
                    p.current_rent = 0;
                } else {
                    p.current_rent = p.rent0;
                }
            }
        } else {
            property->mortgaged = true;
            property->current_rent = 0;
        }
        break;
    }
    case (PropertyType::RAILROAD): {
        int railroads_active = 0;
        for (auto position : this->board_.railroad_positions) {
            int index = this->position_to_properties_[position];
            const PropertyView& railroad = this->properties_[index];
            if (railroad.owner_index == player.player_index && !railroad.mortgaged) {
                railroads_active++;
            }
        }
        assert(railroads_active >= 1);

        const auto& rent_info = this->board_.railroads[0].rent;
        for (auto position : this->board_.railroad_positions) {
            int index = this->position_to_properties_[position];
            PropertyView& railroad = this->properties_[index];
            if (railroad.property_id == property->property_id) {
                railroad.mortgaged = true;
                railroad.current_rent = 0;
            } else if (railroad.owner_index == player.player_index && !railroad.mortgaged) {
                assert(railroads_active >= 2);
                railroad.current_rent = rent_info[railroads_active - 2];
            }
        }
        break;
    }
    case (PropertyType::UTILITY): {
        int utilities_active = 0;
        for (auto position : this->board_.utility_positions) {
            int index = this->position_to_properties_[position];
            const PropertyView& utility = this->properties_[index];
            if (utility.owner_index == player.player_index && !utility.mortgaged) {
                utilities_active++;
            }
        }
        assert(utilities_active >= 1);

        const double average_roll = 7.0;
        const auto& multipliers = this->board_.utilities[0].multiplier;
        for (auto position : this->board_.utility_positions) {
            int index = this->position_to_properties_[position];
            PropertyView& utility = this->properties_[index];
            if (utility.property_id == property->property_id) {
                utility.mortgaged = true;
                utility.current_rent = 0;
            } else if (utility.owner_index == player.player_index && !utility.mortgaged) {
                assert(utilities_active >= 2);
                utility.current_rent = average_roll * multipliers[utilities_active - 2];
            }
        }
        break;
    }}
    player.cash += property->purchase_price / 2;
    return;
}

void Engine::auction(PropertyView* property) {
    while (true) {
        AuctionView auction = {
            property->position,
            0u
        };
        int highest_bidder = -1;

        // Bidding loop
        while (true) {
            bool raised_this_round = false;

            for (int i = 0; i < static_cast<int>(this->agent_adapters_.size()); i++) {
                if (this->players_[i].retired) {
                    continue;
                }

                Action action = this->agent_adapters_[i].auction(&this->state_, &auction);
                if (action.type != ACTION_AUCTION_BID) {
                    this->penalize(&this->players_[i]);
                    continue;
                }

                uint32_t bid = action.auction_bid;
                if (bid <= auction.current_bid) {
                    continue;
                }

                auction.current_bid = bid;
                highest_bidder = static_cast<int>(i);
                raised_this_round = true;
            }

            if (!raised_this_round) {
                break;
            }
        }

        if (highest_bidder < 0 || auction.current_bid == 0) {
            return;
        }

        PlayerView& winner = this->players_[highest_bidder];

        bool can_pay = this->raise_fund(winner, auction.current_bid);
        if (!can_pay) {
            this->bankrupt(winner, nullptr);

            bool any_active = false;
            for (auto const& p : this->players_) {
                if (!p.retired) {
                    any_active = true;
                    break;
                }
            }
            if (!any_active) {
                return;
            }
            continue;
        }

        assert(winner.cash >= auction.current_bid);
        winner.cash -= auction.current_bid;
        property->owner_index = highest_bidder;
        return;
    }
}

void Engine::penalize(PlayerView* player) {
    this->penalties_[player->player_index] += 0.5;
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