#pragma once
#include <stdint.h>
#include <stdbool.h>

// State for each player
typedef struct {
    uint32_t player_index;
    uint32_t cash;
    uint32_t position;
    bool retired;

    // Jail-related
    bool in_jail;
    uint32_t turns_in_jail;
    uint32_t jail_free_cards;
    uint32_t double_rolls;

    // Not used in engine, for agent learning only
    uint8_t railroads_owned;
    uint8_t utilities_owned;
} PlayerView;

typedef enum PropertyType { PROPERTY, UTILITY, RAILROAD }PropertyType;

// State for each property
typedef struct {
    uint32_t position;
    uint32_t property_id; // dont feed this to neural net
    uint32_t owner_index;
    bool is_owned;
    bool mortgaged;
    PropertyType type;

    // Street specific
    uint8_t colour_id;
    uint32_t house_price;
    uint8_t houses;
    bool hotel;

    int32_t purchase_price;
    int32_t rent0, rent1, rent2, rent3, rent4, rentH;

    int32_t current_rent;

    bool auctioned_this_turn;
} PropertyView;

typedef struct {
    uint32_t* properties; // position
    uint8_t property_num;
    uint32_t cash;
    uint32_t jail_cards;
} TradeDetail;

typedef struct {
    uint32_t player_to_offer; // Index of player trying to trade w/, property.owner_index
    TradeDetail offer_from; // things offered by trade proposer
    TradeDetail offer_to; // things trade proposer wants
} TradeOffer;

// Optional struct for auction
typedef struct {
    uint32_t property_id;
    uint32_t current_bid;
} AuctionView;

// Total game state
typedef struct {
    uint32_t game_id;
    uint32_t houses_remaining;
    uint32_t hotels_remaining;
    uint32_t current_player_index;
    const PlayerView* players;
    uint32_t players_remaining; // Use to index players safely
    const PropertyView* properties;
    uint32_t num_properties; // Use to index properties safely
    uint32_t owed;
} GameStateView;