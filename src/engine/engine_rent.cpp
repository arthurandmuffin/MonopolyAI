#include "engine.h"
#include "board.hpp"
#include <cassert>

uint32_t Engine::get_rent(PlayerView& player, bool max_rent) {
    // Get rent using board, assume houses are legal
    // implement functions get_street_rent, get_railroad_rent, get_utility_rent
    const PropertyInfo* street = this->board_.propertyByTile(player.position);
    const int railroadIndex = this->board_.railroadByTile(player.position);
    const int utilityIndex = this->board_.utilityByTile(player.position);
    if (street) {
        return Engine::get_street_rent(player, street);
    } else if (railroadIndex != -1) {
        uint32_t rent = Engine::get_railroad_rent(player, railroadIndex);
        if (max_rent) {
            return 2 * rent;
        }
        return rent;
    } else if (utilityIndex != -1) {
        return Engine::get_utility_rent(player, utilityIndex, max_rent);
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
        if (is_monopoly(street, true)) {
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

uint32_t Engine::get_railroad_rent(PlayerView& player, const int railroadIndex) {
    int index = this->position_to_properties_[player.position];
    PropertyView& railroad = this->properties_[index];
    assert(railroad.type == PropertyType::RAILROAD);

    if (!railroad.is_owned || railroad.mortgaged || railroad.owner_index == player.player_index) {
        return 0;
    }
    return railroad.current_rent;
}

uint32_t Engine::get_utility_rent(PlayerView& player, const int utilityIndex, bool max_rent) {
    int index = this->position_to_properties_[player.position];
    PropertyView& utility = this->properties_[index];
    assert(utility.type == PropertyType::UTILITY);

    if (!utility.is_owned || utility.mortgaged || utility.owner_index == player.player_index) {
        return 0;
    }

    RollResult roll = this->dice_roll();
    PlayerView& utility_owner = players_[utility.owner_index];
    assert(utility_owner.utilities_owned <= 2);
    if (utility_owner.utilities_owned == 1 && !max_rent) {
        return 4 * (roll.roll_1 + roll.roll_2);
    } else {
        return 10 * (roll.roll_1 + roll.roll_2);
    }
}

// active_monopoly to see if all unmortgaged as well
bool Engine::is_monopoly(const PropertyInfo* street, bool active_monopoly) {
    const ColourGroup& colour_group = this->board_.tilesOfColour(street->colour);
    uint32_t owner = -1;
    for (int i = 0; i < colour_group.count; i++) {
        PropertyView& property = this->properties_[this->position_to_properties_[colour_group.tiles[i]]];
        // no monopoly if unowned property
        if (!property.is_owned) {
            return false;
        }

        if (active_monopoly && property.mortgaged) {
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