#include "engine.h"
#include "board.hpp"
#include <cassert>
#include <iostream>

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
    case 2: {
        // Doctor's fee. Pay $50.
        bool payable = this->raise_fund(player, 50);
        if (!payable) {
            this->bankrupt(player, nullptr);
        } else {
            assert(player.cash >= 50);
            player.cash -= 50;
        }
        break;
    }
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
    case 6: {
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
    }
    case 7:
        // Holiday Christmas Fund matures, collect $100
        player.cash += 100;
        break;
    case 8:
        // Income tax refund, collect $20
        player.cash += 20;
        break;
    case 9: {
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
                other_player.cash -= 10;
                player.cash += 10;
            }
        }
        break;
    }
    case 10:
        // Life insurance matures, collect $100
        player.cash += 100;
        break;
    case 11: {
        // Hospital fees, pay $50
        bool payable = this->raise_fund(player, 50);
        if (!payable) {
            this->bankrupt(player, nullptr);
        } else {
            assert(player.cash >= 50);
            player.cash -= 50;
        }
        break;
    }
    case 12: {
        // School fees, pay $50
        bool payable = this->raise_fund(player, 50);
        if (!payable) {
            this->bankrupt(player, nullptr);
        } else {
            assert(player.cash >= 50);
            player.cash -= 50;
        }
        break;
    }
    case 13:
        // Receive consultancy fee of $25
        player.cash += 25;
        break;
    case 14: {
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
    }
    case 15:
        // 2nd place in beauty contest, colelct $10
        player.cash += 10;
        break;
    }
    this->community_deck_.push_back(drawn_card);
}

bool Engine::chance_card_draw(PlayerView& player) {
    uint32_t drawn_card = this->chance_deck_[0];
    this->chance_deck_.erase(this->chance_deck_.begin());
    assert(drawn_card < 16);
    switch (drawn_card)
    {
    case 0:
        // Advance to Boardwalk.
        player.position = 39;
    case 1:
        // Advance to Go & collect $200.
        player.position = 0;
        player.cash += 200;
        break;
    case 2:
        // Advance to Illinois Avenue, collect $200 if pass Go
        if (player.position > 24) {
            player.cash += 200;
        }
        player.position = 24;
    case 3:
        // Advance to St. Charles Place, collect $200 if pass Go
        if (player.position > 11) {
            player.cash += 200;
        }
        player.position = 11;
    case 4:
    case 5: {
        // Advance to the nearest Railroad, double rent if owned already.
        int min_distance = std::numeric_limits<int>::max();
        int8_t closest_railroad = -1;
        for (auto position : this->board_.railroad_positions) {
            int distance = int(player.position) - int(position);
            int abs_distance = std::abs(distance);
            if (abs_distance <= min_distance) {
                min_distance = abs_distance;
                closest_railroad = position;
            }
        }
        assert(closest_railroad > 0);
        player.position = closest_railroad;
        this->chance_deck_.push_back(drawn_card);
        return true;
    }
    case 6: {
        // Advance token to nearest Utility, 10x roll.
        int min_distance = std::numeric_limits<int>::max();
        int8_t closest_utility = -1;
        for (auto position : this->board_.utility_positions) {
            int distance = int(player.position) - int(position);
            int abs_distance = std::abs(distance);
            if (abs_distance <= min_distance) {
                min_distance = abs_distance;
                closest_utility = position;
            }
        }
        assert(closest_utility > 0);
        player.position = closest_utility;
        this->chance_deck_.push_back(drawn_card);
        return true;
    }
    case 7:
        // Bank pays you dividend of $50
        player.cash += 50;
        break;
    case 8:
        // Get out of jail free card
        player.jail_free_cards += 1;
        return false;
    case 9:
        // Go Back 3 Spaces
        player.position -= 3;
        assert(player.position >= 0);
        break;
    case 10:
        // Go to jail
        this->jail(player);
        break;
    case 11: {
        // Street repairs, pay $20 per house and $100 per hotel
        uint32_t repair_cost = 0;
        for (auto& property : this->properties_) {
            if (property.owner_index != player.player_index || property.type != PropertyType::PROPERTY) {
                continue;
            }
            if (property.hotel) {
                repair_cost += 100;
            } else {
                repair_cost += 25 * property.houses;
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
    }
    case 12: {
        // Speeding fine $15
        bool payable = this->raise_fund(player, 15);
        if (!payable) {
            this->bankrupt(player, nullptr);
        } else {
            assert(player.cash >= 15);
            player.cash -= 15;
        }
        break;
    }
    case 13:
        // Take a trip to Reading Railroad. If you pass Go, collect $200.
        if (player.position > 5) {
            player.cash += 200;
        }
        player.position = 5;
    case 14: {
        // You have been elected Chairman of the Board. Pay each player $50.
        for (auto& other_player : this->players_) {
            if (player.player_index == other_player.player_index || other_player.retired) {
                continue;
            }
            bool payable = this->raise_fund(player, 50);
            if (!payable) {
                this->bankrupt(player, &other_player);
            } else {
                assert(player.cash >= 50);
                player.cash -= 50;
                other_player.cash += 50;
            }
        }
        break;
    }
    case 15:
        // Your building loan matures. Collect $150
        player.cash += 150;
        break;
    }
    this->chance_deck_.push_back(drawn_card);
    return false;
}