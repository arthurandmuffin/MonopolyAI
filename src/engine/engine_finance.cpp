#include "engine.h"
#include "board.hpp"
#include <cassert>
#include <set>

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

                if (is_monopoly(asset_info, true)) {
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

void Engine::unmortgage(PlayerView& player, PropertyView* property) {
    uint32_t unmortgage_cost = static_cast<uint32_t>((property->purchase_price / 2) * 1.1);
    assert(property->mortgaged);
    assert(property->owner_index == player.player_index);
    assert(player.cash >= unmortgage_cost);
    assert(property->houses == 0);

    player.cash -= unmortgage_cost;
    property->mortgaged = false;

    switch (property->type) {
    case (PropertyType::PROPERTY): {
        const PropertyInfo* property_info = this->board_.propertyByTile(property->position);
        if (is_monopoly(property_info, true)) {
            const ColourGroup group = this->board_.tilesOfColour(property_info->colour);
            for (int i = 0; i < group.count; i++) {
                int index = this->position_to_properties_[group.tiles[i]];
                PropertyView& p = this->properties_[index];
                assert(p.owner_index == player.player_index);
                if (p.houses == 0) {
                    p.current_rent = p.rent0 * 2;
                }
            }
        }
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
            if (railroad.owner_index == player.player_index) {
                railroad.current_rent = rent_info[railroads_active - 1];
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
            if (utility.owner_index == player.player_index) {
                utility.current_rent = average_roll * multipliers[utilities_active - 1];
            }
        }
        break;
    }
    }
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
        if (is_monopoly(asset_info, true)) {
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
    assert(property->houses == 0);
    property->owner_index = -1;
    property->is_owned = false;
    while (true) {
        int index = this->position_to_properties_[property->position];
        AuctionView auction = {
            index,
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
                    this->penalize(this->players_[i]);
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
        property->is_owned = true;
        return;
    }
}

void Engine::bankrupt(PlayerView& player, PlayerView* debtor) {
    if (debtor) {
        debtor->cash += player.cash;
    }

    player.cash = 0;
    player.retired = true;
    player.in_jail = false;
    player.turns_in_jail = 0;
    player.jail_free_cards = 0;
    player.double_rolls = 0;
    player.railroads_owned = 0;
    player.utilities_owned = 0;

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