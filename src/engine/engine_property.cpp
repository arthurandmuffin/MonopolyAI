#include "engine.h"
#include "board.hpp"
#include <cassert>

void Engine::buy_property(PlayerView& player, PropertyView* property) {
    assert(property->owner_index == -1);
    bool payable = this->raise_fund(player, property->purchase_price);
    if (!payable) {
        this->bankrupt(player, nullptr);
    } else {
        player.cash -= property->purchase_price;
        property->owner_index = player.player_index;
    }
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