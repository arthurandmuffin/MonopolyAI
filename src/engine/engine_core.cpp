#include "engine.h"
#include "board.hpp"
#include <fstream>
#include <cstdint>
#include <set>
#include <cassert>
#include <algorithm>
#include <iostream>

GameResult Engine::run() {
    std::cerr << "Running agent\n";
    int turn = 0;
    int winner = -1;
    while (turn < cfg_.max_turns)
    {
        assert(players_.size() == agent_adapters_.size());
        int active_players = 0;
        int active_index = -1;
        for (size_t i = 0; i < this->players_.size(); i++) {
            PlayerView& player = this->players_[i];
            if (!player.retired) {
                active_index = i;
                active_players++;
            }
        }

        if (active_players <= 1) {
            winner = active_index;
            break;
        }
        for (auto it = this->players_.begin(); it != this->players_.end(); it++) {
            auto& player = *it;
            if (player.retired) {
                continue;
            }

            if (player.turns_in_jail == 2) {
                player.turns_in_jail = 0;
                player.in_jail = false;
            }

            if (!this->in_jail(player)) {
                RollResult dice_roll = this->dice_roll();
                bool in_jail = update_position(player, dice_roll);
                if (in_jail) {
                    continue;
                }
                this->handle_position(player);
                if (player.retired) {
                    continue;
                }
            } else {
                player.turns_in_jail++;
            }

            uint32_t index = player.player_index;
            AgentAdapter& agent = this->agent_adapters_[index];
            while (true) {
                Action agent_action = agent.agent_turn(&this->state_);
                if (this->handle_action(player, agent_action)) {
                    player.trades_offered = 0;
                    player.previous_offer = {};
                    player.offer_accepted = false;
                    break;
                }
                if (player.retired) {
                    break;
                }
            }
            

            int property_index = this->position_to_properties_[player.position];
            if (property_index != -1) {
                PropertyView& property_on = this->properties_[property_index];
                property_on.auctioned_this_turn = false;
            }
            player.jail_rolled_this_turn = false;
        }
        turn++;
    }

    
    
    GameResult result = {
        this->cfg_.game_id,
        static_cast<uint64_t>(turn),
        winner,
        this->get_player_scores(),
        this->penalties_,
        this->state_,
    };

    return result;
}

bool Engine::handle_action(PlayerView& player, Action player_action) {
    switch (player_action.type) {
    case (ActionType::ACTION_LANDED_PROPERTY): {
        int index = this->position_to_properties_[player.position];
        if (player_action.buying_property) {
            if (index == -1) {
                this->penalize(player);
                return true;
            } else {
                PropertyView* property = &this->properties_[index];
                if (property->owner_index != -1) {
                    this->penalize(player);
                    return true;
                }
                this->buy_property(player, property);
            }
        } else {
            if (index == -1) {
                this->penalize(player);
                return true;
            }else {
                PropertyView* property = &this->properties_[index];
                if (property->auctioned_this_turn) {
                    this->penalize(player);
                    property->auctioned_this_turn = false;
                    return true;
                }
                property->auctioned_this_turn = true;
                this->auction(property);
            }
        }
        break;
    }
    case (ActionType::ACTION_TRADE): {
        TradeOffer& offer = player_action.trade_offer;
        uint32_t player_index_to_offer = offer.player_to_offer;
        PlayerView& player_to_offer = this->players_[player_index_to_offer];
        TradeDetail& assets_offered = offer.offer_from;
        TradeDetail& assets_demanded = offer.offer_to;

        player.trades_offered++;
        player.previous_offer = offer;

        if (player.trades_offered >= 10) {
            // max 10 offers per turn
            this->penalize(player);
            return true;
        }

        if (player_to_offer.retired) {
            // cant offer a trade w/ player out of game
            this->penalize(player);
            return true;
        }

        if (player.player_index == player_to_offer.player_index) {
            // cant trade w/ yourself
            this->penalize(player);
            return true;
        }

        if (!this->legal_trade_detail(player, assets_offered) || !this->legal_trade_detail(player_to_offer, assets_demanded)) {
            this->penalize(player);
            return true;
        }

        Action trade_response = this->agent_adapters_[player_index_to_offer].trade_offer(&this->state_, &offer);
        if (trade_response.type != ACTION_TRADE_RESPONSE) {
            this->penalize(player_to_offer);
            return true;
        }

        if (trade_response.trade_response) {
            this->trade(player, assets_offered, player_to_offer, assets_demanded);
            player.offer_accepted = true;
        } else {
            player.offer_accepted = false;
        }

        break;
    }
    case (ActionType::ACTION_TRADE_RESPONSE):
        // Should not be an action player sends on their own, must be prompted
        this->penalize(player);
        return true;
    case (ActionType::ACTION_MORTGAGE): {
        uint32_t position = player_action.property_position;
        int index = this->position_to_properties_[position];
        if (index == -1) {
            // not a property
            this->penalize(player);
            return true;
        }

        PropertyView* property = &this->properties_[index];
        if (property->owner_index != player.player_index || property->houses > 0 || property->mortgaged) {
            this->penalize(player);
            return true;
        }

        this->mortgage(player, property);
        break;
    }
    case (ActionType::ACTION_UNMORTGAGE): {
        uint32_t position = player_action.property_position;
        int index = this->position_to_properties_[position];
        if (index == -1) {
            // not a property
            this->penalize(player);
            return true;
        }

        PropertyView* property = &this->properties_[index];
        if (property->owner_index != player.player_index || !property->mortgaged) {
            this->penalize(player);
            return true;
        }

        if (player.cash <= (property->purchase_price / 2) * 1.1) {
            this->penalize(player);
            return true;
        }

        this->unmortgage(player, property);
        break;
    }
    case (ActionType::ACTION_DEVELOP): {
        uint32_t position = player_action.property_position;
        int index = this->position_to_properties_[position];
        if (index == -1) {
            this->penalize(player);
            return true;
        }

        PropertyView* property = &this->properties_[index];
        if (property->owner_index != player.player_index || property->houses == 5) {
            // Does not own property / property already fully developed
            this->penalize(player);
            return true;
        }

        const PropertyInfo* property_info = this->board_.propertyByTile(property->position);

        if (!this->is_monopoly(property_info, true)) {
            // Not a monopoly, cant build
            this->penalize(player);
            return true;
        }

        if (player.cash < property->house_price) {
            // cannot afford a house
            this->penalize(player);
            return true;
        }

        this->build_house(player, property);
        break;
    }
    case (ActionType::ACTION_UNDEVELOP): {
        uint32_t position = player_action.property_position;
        int index = this->position_to_properties_[position];
        if (index == -1) {
            this->penalize(player);
            return true;
        }

        PropertyView* property = &this->properties_[index];
        if (property->owner_index != player.player_index || property->houses == 0) {
            // Does not own property / no houses on property
            this->penalize(player);
            return true;
        }

        this->sell_house(player, property);
        break;
    }
    case (ActionType::ACTION_AUCTION_BID):
        // Should not be an action player sends on their own, must be prompted
        this->penalize(player);
        return true;
    case (ActionType::ACTION_END_TURN):
        return true;
    case (ActionType::ACTION_PAY_JAIL_FINE): {
        if (!this->in_jail(player)) {
            this->penalize(player);
            return true;
        }

        if (player.cash < 50) {
            this->penalize(player);
            return true;
        }

        player.cash -= 50;
        
        RollResult dice_roll = this->dice_roll();
        bool in_jail = update_position(player, dice_roll);
        if (in_jail) {
            return true;
        }
        this->handle_position(player);
        break;
    }
    case (ActionType::ACTION_USE_JAIL_CARD): {
        if (!this->in_jail(player)) {
            this->penalize(player);
            return true;
        }

        if (player.jail_free_cards == 0) {
            this->penalize(player);
            return true;
        }

        this->use_jail_free_card(player);
        
        RollResult dice_roll = this->dice_roll();
        this->update_position(player, dice_roll);
        this->handle_position(player);

        if (this->in_jail(player)) {
            // jail after handle, community / chance
            return true;
        }
        break;
    }
    case (ActionType::ACTION_JAIL_ROLL_DOUBLE): {
        if (!this->in_jail(player) || player.jail_rolled_this_turn) {
            this->penalize(player);
            return true;
        }
        
        RollResult dice_roll = this->dice_roll();
        player.jail_rolled_this_turn = true;
        if (!dice_roll.is_double) {
            break;
        }
        this->update_position(player, dice_roll);
        this->handle_position(player);

        if (this->in_jail(player)) {
            // somehow in jail again
            return true;
        }
        break;
    }
    }
    return false;
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
    bool max_rent = false;
    switch (this->board_.tiles[player.position].type) {
    case (TileType::Chance):
        max_rent = this->chance_card_draw(player);
        if (player.retired) {
            return;
        }
        break;
    case (TileType::Community):
        this->community_card_draw(player);
        if (player.retired) {
            return;
        }
        break;
    default:
        break;
    }
    uint32_t rent = 0;
    uint32_t cost = 0;
    PlayerView* debtor = nullptr;
    switch (this->board_.tiles[player.position].type) {
    case (TileType::Property):
    case (TileType::Railroad):
    case (TileType::Utility): {
        int index = this->position_to_properties_[player.position];
        PropertyView& property = this->properties_[index];
        debtor = &this->players_[property.owner_index];
        rent = get_rent(player, max_rent);
        break;
    }
    case (TileType::Tax): {
        cost = 200; //config, implement 10% in future
        break;
    }
    case (TileType::GoToJail):
        this->jail(player);
        break;
    default:
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
        bool payable = this->raise_fund(player, cost);
        if (payable) {
            assert(player.cash >= cost);
            player.cash -= cost;
        } else {
            this->bankrupt(player, nullptr);
        }
    }
}

void Engine::penalize(PlayerView& player) {
    this->penalties_[player.player_index] += 0.5;
}

void Engine::jail(PlayerView& player) {
    player.in_jail = true;
    player.turns_in_jail = 0;
    player.position = this->board_.jailPosition();
    return;
}

bool Engine::in_jail(PlayerView& player) {
    return player.in_jail;
}

void Engine::use_jail_free_card(PlayerView& player) {
    player.jail_free_cards -= 1;
    assert(this->community_deck_.size() != 16 || this->chance_deck_.size() != 16);

    if (this->community_deck_.size() != 16) {
        this->community_deck_.push_back(4);
    } else {
        this->chance_deck_.push_back(8);
    }
}

RollResult Engine::dice_roll() {
    int roll1 = dice_(rng_);
    int roll2 = dice_(rng_);
    return {roll1, roll2, roll1 == roll2};
}

std::vector<double> Engine::get_player_scores() {
    std::vector<double> scores;

    for (int i = 0; i < this->players_.size(); i++) {
        scores.push_back(this->networth(players_[i]) + 4 * this->expected_income(players_[i]));
    }
    return scores;
}

double Engine::networth(PlayerView& player) {
    double networth = 0;

    networth += player.cash;
    
    for (PropertyView& property : this->properties_) {
        if (property.owner_index != player.player_index) {
            continue;
        }

        double property_worth = 0;
        property_worth += property.purchase_price;
        if (property.type == PropertyType::PROPERTY) {
            property_worth += property.house_price * property.houses;
        }
        networth += property_worth;
    }
    return networth;
}

double Engine::expected_income(PlayerView& player) {
    double expected_income = 0;

    for (PropertyView& property : this->properties_) {
        if (property.owner_index != player.player_index) {
            continue;
        }
        expected_income += this->board_.tile_probability[property.position] * property.current_rent;
    }
    return expected_income;
}