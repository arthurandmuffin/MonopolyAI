#include "agent_abi.h"
#include "state_view.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct {
    uint32_t agent_index;
    uint64_t seed;
    uint32_t rand_state;
    char name[64];
} RandomAgent;

// Simple Linear Congruential Generator for consistent randomness
uint32_t next_random(RandomAgent* agent) {
    agent->rand_state = agent->rand_state * 1103515245 + 12345;
    return agent->rand_state;
}

// Random float between 0.0 and 1.0
float random_float(RandomAgent* agent) {
    return (float)(next_random(agent) % 10000) / 10000.0f;
}

// Random int between min and max (inclusive)
uint32_t random_range(RandomAgent* agent, uint32_t min, uint32_t max) {
    if (min >= max) return min;
    return min + (next_random(agent) % (max - min + 1));
}

int abi_version() {
    return ABI_VERSION;
}

void* create_agent_instance(const char* config_json) {
    RandomAgent* agent = malloc(sizeof(RandomAgent));
    if (!agent) {
        return NULL;
    }

    agent->agent_index = 0;
    agent->seed = 0;
    agent->rand_state = (uint32_t)time(NULL); // Initialize with current time
    strncpy(agent->name, "RandomNaive", sizeof(agent->name) - 1);
    agent->name[sizeof(agent->name) - 1] = '\0';
    return agent;
}

void destroy_agent(void* agent_ptr) {
    if (agent_ptr) {
        free(agent_ptr);
    }
}

void game_start(void* agent_ptr, uint32_t agent_index, uint64_t seed) {
    RandomAgent* agent = (RandomAgent*)agent_ptr;
    if (!agent) return;
    
    agent->agent_index = agent_index;
    agent->seed = seed;
    agent->rand_state = (uint32_t)(seed + agent_index); // Seed with game seed + player index
}

Action agent_turn(void* agent_ptr, const GameStateView* state) {
    RandomAgent* agent = (RandomAgent*)agent_ptr;
    Action action = {0};
    
    if (!agent || !state || agent->agent_index >= state->players_remaining) {
        action.type = ACTION_END_TURN;
        return action;
    }
    
    const PlayerView* me = &state->players[agent->agent_index];
    
    // Find property at current position
    const PropertyView* current_property = NULL;
    for (uint32_t i = 0; i < state->num_properties; i++) {
        if (state->properties[i].property_id == me->position) {
            current_property = &state->properties[i];
            break;
        }
    }
    
    if (current_property && !current_property->is_owned) {
        uint32_t price = current_property->purchase_price;
        
        if (me->cash >= price) {
            // Random decision with bias based on affordability
            float affordability = (float)price / (float)me->cash;
            float buy_probability;
            
            if (affordability <= 0.2f) {
                buy_probability = 0.8f; // 80% chance if very affordable
            } else if (affordability <= 0.4f) {
                buy_probability = 0.6f; // 60% chance if affordable
            } else if (affordability <= 0.6f) {
                buy_probability = 0.3f; // 30% chance if expensive
            } else {
                buy_probability = 0.1f; // 10% chance if very expensive
            }
            
            bool should_buy = random_float(agent) < buy_probability;
            
            action.type = ACTION_LANDED_PROPERTY;
            action.buying_property = should_buy;
        } else {
            action.type = ACTION_LANDED_PROPERTY;
            action.buying_property = false;
        }
    } else {
        action.type = ACTION_END_TURN;
    }
    
    return action;
}

Action auction(void* agent_ptr, const GameStateView* state, const AuctionView* auction) {
    RandomAgent* agent = (RandomAgent*)agent_ptr;
    Action action = {0};
    
    if (!agent || !state || !auction) {
        action.type = ACTION_END_TURN;
        return action;
    }
    
    const PlayerView* me = &state->players[agent->agent_index];
    
    // Find the property being auctioned
    const PropertyView* prop = NULL;
    for (uint32_t i = 0; i < state->num_properties; i++) {
        if (state->properties[i].property_id == auction->property_id) {
            prop = &state->properties[i];
            break;
        }
    }
    
    if (!prop) {
        action.type = ACTION_END_TURN;
        return action;
    }
    
    // Random bidding strategy
    float bid_probability = 0.4f; // 40% chance to bid
    
    if (random_float(agent) < bid_probability) {
        // Bid a random amount between 10% and 50% of property value
        uint32_t min_bid = (prop->purchase_price * 10) / 100;
        uint32_t max_bid = (prop->purchase_price * 50) / 100;
        
        // Don't bid more than 30% of our cash
        uint32_t cash_limit = (me->cash * 30) / 100;
        if (max_bid > cash_limit) {
            max_bid = cash_limit;
        }
        
        if (max_bid >= min_bid && max_bid >= 10) {
            uint32_t bid = random_range(agent, min_bid, max_bid);
            
            action.type = ACTION_AUCTION_BID;
            action.auction_bid = bid;
        } else {
            action.type = ACTION_END_TURN;
        }
    } else {
        action.type = ACTION_END_TURN;
    }
    
    return action;
}

Action trade_offer(void* agent_ptr, const GameStateView* state, const TradeOffer* offer) {
    RandomAgent* agent = (RandomAgent*)agent_ptr;
    Action action = {0};
    
    if (!agent || !state || !offer) {
        action.type = ACTION_TRADE_RESPONSE;
        action.trade_response = false;
        return action;
    }
    
    // Random decision with slight bias toward rejection (realistic)
    bool accept = random_float(agent) < 0.3f; // 30% chance to accept
    
    action.type = ACTION_TRADE_RESPONSE;
    action.trade_response = accept;
        
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