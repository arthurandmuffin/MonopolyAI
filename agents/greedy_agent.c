#include "agent_abi.h"
#include "state_view.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>

// Greedy naive agent implementation

typedef struct {
    uint32_t agent_index;
    uint64_t seed;
    char name[64];
} GreedyAgent;

typedef struct {
    uint8_t total_properties;
    uint8_t owned_properties;
    uint8_t opponent_owned_properties;
} PropertyInfo;

int abi_version() {
    return ABI_VERSION;
}

void* create_agent_instance(const char* config_json) {
    GreedyAgent* agent = (GreedyAgent*)malloc(sizeof(GreedyAgent));
    if (!agent) {
        return NULL;
    }

    agent->agent_index = 0;
    agent->seed = 0;
    strncpy(agent->name, "GreedyAgent", sizeof(agent->name)-1);
    agent->name[sizeof(agent->name)-1] = '\0';
    return agent;
}

void destroy_agent(void* agent_ptr) {
    if (agent_ptr) {
        free(agent_ptr);
    }
}

void game_start(void* agent_ptr, uint32_t agent_index, uint64_t seed) {
    GreedyAgent* agent = (GreedyAgent*)agent_ptr;
    if (!agent) {
        return;
    }
    agent->agent_index = agent_index;
    agent->seed = seed;
}

// Helper to check property info
static PropertyInfo analyze_property_ownership(const GameStateView* state, uint32_t colour_id, uint32_t agent_index) {
    PropertyInfo info = {0, 0, 0};
    if (colour_id >= state->num_properties) {
        return info; // Invalid property
    }

    // Count total properties of this color
    for (uint32_t i = 0; i < state->num_properties; ++i) {
        const PropertyView* property = &state->properties[i];
        if (property->colour_id == colour_id) {
            info.total_properties++;
            if (property->is_owned) {
                if (property->owner_index == agent_index) {
                    info.owned_properties++;
                } else {
                    info.opponent_owned_properties++;
                }
            }
        }
    }
    return info;
}

// Helper to calculate property value ratio
float calculate_property_value(const GameStateView* state, uint32_t property_id, uint32_t agent_index) {
    if (property_id >= state->num_properties) {
        return 0.0f; // Invalid property
    }
    const PropertyView* property = NULL;
    for (uint32_t i = 0; i < state->num_properties; ++i) {
        if (state->properties[i].property_id == property_id) {
            property = &state->properties[i];
            break;
        }
    }
    if (!property) {
        return 0.0f; // Property not found
    }

    float base_value = 0.0f;
    
    // Property types PROPERTY, RAILROAD, UTILITY
    switch (property->type){
        case PROPERTY:{
            base_value = (float)property->rent0;
            PropertyInfo info = analyze_property_ownership(state, property->colour_id, agent_index);
            if (info.owned_properties == info.total_properties - 1 && info.opponent_owned_properties == 0) {
                base_value *= 5.0f; // Monopoly bonus
            } else if (info.owned_properties > 0 && info.opponent_owned_properties == 0) {
                base_value *= 2.0f; // Partial ownership bonus
            } else if (info.opponent_owned_properties > 0) {
                base_value *= 0.5f; // Opponent ownership penalty
            }
            break;
        }
        case RAILROAD:{
            // Value increases with number owned
            const PlayerView* player = &state->players[agent_index];
            float railroad_values[] = {25.0f, 50.0f, 100.0f, 200.0f};
            base_value = (player->railroads_owned < 4) ? railroad_values[player->railroads_owned] : 200.0f;
            break;
        }
        case UTILITY: {
            // More value if 1 already owned
            const PlayerView* player = &state->players[agent_index];
            base_value = (player->utilities_owned == 0) ? 10.0f : 20.0f;
            break;
        }
    }

    return base_value / (float)property->purchase_price;
}

// Helper to find least valuable property to mortgage
uint32_t property_to_mortgage(const GameStateView* state, uint32_t agent_index, uint32_t needed_amount) {
    uint32_t best_pid = UINT32_MAX;
    float worst_value = FLT_MAX;
    for (uint32_t i = 0; i < state->num_properties; i++) {
        const PropertyView* property = &state->properties[i];
        if (property->is_owned && property->owner_index == agent_index && !property->mortgaged) {
            float value = calculate_property_value(state, property->property_id, agent_index);
            if (value < worst_value) {
                worst_value = value;
                best_pid = property->property_id;
            }
        }
    }
    return best_pid;
}

Action agent_turn(void* agent_ptr, const GameStateView* state) {
    GreedyAgent* agent = (GreedyAgent*)agent_ptr;
    Action action = {0};
    // Validate agent and state
    if (!agent || !state || agent->agent_index >= state->players_remaining) {
        action.type = ACTION_END_TURN;
        return action;
    }
    const PlayerView* player = &state->players[agent->agent_index];

    // Jail handling
    if (player->in_jail) {
        if (player->jail_free_cards > 0){
            action.type = ACTION_USE_JAIL_CARD;
            return action;

        } else if (player->cash >= 50){
            action.type = ACTION_PAY_JAIL_FINE;
            return action;
            
        }
    }

    /* Mortgage handling handled by engine
    // Handle payment when mortgage needed
    if (state->owed > 0 && state->owed > player->cash) {
        // Mortgage properties until enough cash
        uint32_t mortgage_property = property_to_mortgage(state, agent->agent_index, state->owed - player->cash);

        if (mortgage_property != UINT32_MAX) {
            action.type = ACTION_MORTGAGE;
            action.mortgage_property = mortgage_property;
            return action;
        } else {
            // no more properties to mortgage, declare bankruptcy
            action.type = ACTION_END_TURN;
            printf("Agent %s declares bankruptcy!\n", agent->name);
            return action;
        }
    }
        */

    // Buying unowned property
    const PropertyView* landed_property = NULL;
    for (uint32_t i = 0; i < state->num_properties; ++i) {
        if (state->properties[i].position == player->position) {
            landed_property = &state->properties[i];
            break;
        }
    }

    if (landed_property && !landed_property->is_owned) {
        uint32_t price = landed_property->purchase_price;
        // Buy property if affordable
        if (player->cash >= price) { 
            action.type = ACTION_LANDED_PROPERTY;
            action.buying_property = true;
            return action;
        } else {
            action.type = ACTION_LANDED_PROPERTY;
            action.buying_property = false;
            return action;
        }
    }

    // Default action: end turn
    action.type = ACTION_END_TURN;
    return action;
}

Action auction(void* agent_ptr, const GameStateView* state, const AuctionView* auction) {
    GreedyAgent* agent = (GreedyAgent*)agent_ptr;
    Action action = {0};

    // Validate agent, state, and auction
    if (!agent || !state || !auction) {
        action.type = ACTION_END_TURN;
        return action;
    }

    const PlayerView* player = &state->players[agent->agent_index];
    
    // Auction startegy: bid up to 90% of calculated property value
    float property_value = calculate_property_value(state, auction->property_id, agent->agent_index);
    const PropertyView* property = NULL;
    for (uint32_t i = 0; i < state->num_properties; ++i) {
        if (state->properties[i].property_id == auction->property_id) {
            property = &state->properties[i];
            break;
        }
    }

    if (!property) {
        action.type = ACTION_END_TURN;
        return action;
    }
    // Calculate max bid and cash limit
    uint32_t max_bid = (uint32_t)((float)property->purchase_price * property_value * 0.90f);
    
    if (max_bid >= 10 && property_value >= 0.1f) {
        action.type = ACTION_AUCTION_BID;
        action.auction_bid = max_bid;
    } else {
        action.type = ACTION_END_TURN;
    }

    return action;
}

Action trade_offer(void* agent_ptr, const GameStateView* state, const TradeOffer* offer) {
    GreedyAgent* agent = (GreedyAgent*)agent_ptr;
    Action action = {0};
    action.type = ACTION_TRADE_RESPONSE;

    if (!agent || !state || !offer) {
        action.trade_response = false;
        return action;
    }

    // Accept all trade offers (greedy strategy)
    action.trade_response = true;
    return action;
}

// Export vtable
AgentVTable vtable = {
    .abi_version = abi_version,
    .create_agent = create_agent,
    .destroy_agent = destroy_agent,
    .game_start = game_start,
    .agent_turn = agent_turn,
    .auction = auction,
    .trade_offer = trade_offer
};

AGENT_API AgentExport create_agent(const char* config_json) {
    AgentExport export;
    export.vtable = vtable;
    return export;
}
