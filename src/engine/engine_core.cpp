#include "engine.h"
#include "board.hpp"
#include <fstream>
#include <cstdint>
#include <set>
#include <cassert>
#include <algorithm>

GameResult Engine::run() {
    int turn = 0;
    while (turn < cfg_.max_turns)
    {
        for (auto it = this->players_.begin(); it != this->players_.end(); it++) {
            auto& player = *it;
            if (player.retired) {
                continue;
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
            }

            uint32_t index = player.player_index;
            AgentAdapter& agent = this->agent_adapters_[index];
            bool turn_end = false;
            while (true) {
                Action agent_action = agent.agent_turn(&this->state_);
                if (this->handle_action(player, agent_action)) {
                    break;
                }
            }
        }
        turn++;
    }
}

bool Engine::handle_action(PlayerView& player, Action player_action) {
    switch (player_action.type) {
    case (ActionType::ACTION_LANDED_PROPERTY):
        int index = this->position_to_properties_[player.position];
        if (player_action.buying_property) {
            if (index == -1) {
                this->penalize(player);
            } else {
                PropertyView* property = &this->properties_[index];
                if (property->owner_index != -1) {
                    this->penalize(player);
                }
                this->buy_property(player, property);
            }
        } else {
            if (index != -1) {
                PropertyView* property = &this->properties_[index];
                this->auction(property);
            }
        }
        break;
    case (ActionType::ACTION_TRADE):
        break;
    case (ActionType::ACTION_TRADE_RESPONSE):
        // Should not be an action player sends on their own, must be prompted
        this->penalize(player);
        break;
    case (ActionType::ACTION_MORTGAGE):
        uint32_t position = player_action.property_position;
        int index = this->position_to_properties_[position];
        if (index == -1) {
            // not a property
            this->penalize(player);
            break;
        }

        PropertyView* property = &this->properties_[index];
        if (property->owner_index != player.player_index || property->houses > 0 || property->mortgaged) {
            this->penalize(player);
            break;
        }

        this->mortgage(player, property);
        break;
    case (ActionType::ACTION_DEVELOP):
        return true;
    case (ActionType::ACTION_UNDEVELOP):
        return true;
    case (ActionType::ACTION_AUCTION_BID):
        // Should not be an action player sends on their own, must be prompted
        this->penalize(player);
        break;
    case (ActionType::ACTION_END_TURN):
        return true;
    case (ActionType::ACTION_PAY_JAIL_FINE):
        break;
    case (ActionType::ACTION_USE_JAIL_CARD):
        break;
    case (ActionType::ACTION_JAIL_ROLL_DOUBLE):
        break;
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
    return player.position == static_cast<uint32_t>(this->board_.jailPosition());
}

RollResult Engine::dice_roll() {
    int roll1 = dice_(rng_);
    int roll2 = dice_(rng_);
    return {roll1, roll2, roll1 == roll2};
}