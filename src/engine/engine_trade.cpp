#include "engine.h"
#include "board.hpp"

void Engine::trade(PlayerView& playerA, TradeDetail& playerA_assets, PlayerView& playerB, TradeDetail& playerB_assets) {
    // playerA_assets -> playerB
    playerA.cash -= playerA_assets.cash;
    playerB.cash += playerA_assets.cash;

    playerA.jail_free_cards -= playerA_assets.jail_cards;
    playerB.jail_free_cards += playerA_assets.jail_cards;

    for (uint8_t i = 0; i < playerA_assets.property_num; i++) {
        uint32_t position = playerA_assets.properties[i];
        int index = this->position_to_properties_[position];
        PropertyView& asset = this->properties_[index];
        asset.owner_index = playerB.player_index;
        if (asset.type == PropertyType::RAILROAD) {
            playerB.railroads_owned++;
            playerA.railroads_owned--;
        } else if (asset.type == PropertyType::UTILITY) {
            playerB.utilities_owned++;
            playerA.utilities_owned--;
        }
        this->update_rent(asset);
    }

    // playerB_assets -> player A

    playerB.cash -= playerB_assets.cash;
    playerA.cash += playerB_assets.cash;

    playerB.jail_free_cards -= playerB_assets.jail_cards;
    playerA.jail_free_cards += playerB_assets.jail_cards;

    for (uint8_t i = 0; i < playerB_assets.property_num; i++) {
        uint32_t position = playerB_assets.properties[i];
        int index = this->position_to_properties_[position];
        PropertyView& asset = this->properties_[index];
        asset.owner_index = playerA.player_index;
        if (asset.type == PropertyType::RAILROAD) {
            playerA.railroads_owned++;
            playerB.railroads_owned--;
        } else if (asset.type == PropertyType::UTILITY) {
            playerA.utilities_owned++;
            playerB.utilities_owned--;
        }
        this->update_rent(asset);
    }
}

bool Engine::legal_trade_detail(PlayerView& player, TradeDetail& assets) {
    if (assets.property_num == 0 && assets.cash == 0 && assets.jail_cards == 0) {
        return false;
    }

    if (player.cash < assets.cash) {
        return false;
    }

    if (player.jail_free_cards < assets.jail_cards) {
        return false;
    }

    for (int i = 0; i < assets.property_num; i++) {
        uint32_t asset_position = assets.properties[i];
        int index = this->position_to_properties_[asset_position];
        if (index == -1) {
            return false;
        }
        PropertyView* property = &this->properties_[index];
        if (property->owner_index != player.player_index) {
            return false;
        }
    }
    return true;
}