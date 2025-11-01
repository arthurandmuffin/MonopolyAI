#pragma once
#include <stdint.h>
#include <stdbool.h>

// State for each player
typedef struct {
    uint32_t player_index;
    uint32_t cash;
    uint32_t position;
    bool retired;
    bool in_jail;
    uint32_t jail_free_cards;
} PlayerView;

// State for each property
typedef struct {
    uint32_t property_id;
    uint32_t owner_index;
    bool owned;
    uint8_t colour_id;
    uint8_t houses;
    uint8_t hotel;
    int32_t purchase_price;
    int32_t rent0, rent1, rent2, rent3, rent4, rentH;
} PropertyView;

typedef struct {
    uint32_t* property_ids;
    uint8_t property_num;
    uint32_t cash;
    uint32_t jail_cards;
} TradeDetail;

typedef struct {
    uint32_t prop_player_index;
    TradeDetail offer_from;
    TradeDetail offer_to;
} TradeOffer;

// Optional struct for auction
typedef struct {
    uint32_t property_id;
} AuctionView;

// Total game state
typedef struct {
    uint32_t game_id;
    uint32_t players_remaining;
    uint32_t num_properties;
    uint32_t houses_remaining;
    uint32_t hotels_remaining;
    const PlayerView* players;
    const PropertyView* properties;
} GameStateView;