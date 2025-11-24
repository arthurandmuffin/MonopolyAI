#include "engine.h"
#include "board.hpp"
#include <cassert>

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