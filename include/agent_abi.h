#pragma once
#include "state_view.h"

#ifdef _WIN32
  #ifdef AGENT_BUILD
    #define AGENT_API __declspec(dllexport)
  #else
    #define AGENT_API __declspec(dllimport)
  #endif
#else
  #define AGENT_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ABI_VERSION 1

typedef enum {
    ACTION_LANDED_PROPERTY,
    ACTION_TRADE,
    ACTION_TRADE_RESPONSE,
    ACTION_MORTGAGE,
    ACTION_UNMORTGAGE,
    ACTION_DEVELOP,
    ACTION_UNDEVELOP,
    ACTION_AUCTION_BID,
    ACTION_END_TURN,
    ACTION_PAY_JAIL_FINE,
    ACTION_USE_JAIL_CARD,
    ACTION_JAIL_ROLL_DOUBLE
} ActionType;

typedef struct {
    ActionType type;
    union {
        bool buying_property;
        TradeOffer trade_offer;
        bool trade_response;
        uint32_t property_position; // position of property to build/unbuild houses and mortgage
        uint32_t auction_bid;
    };
} Action;

typedef struct AgentVTable {
    int (*abi_version)(); // Check agent has same headerfile

    // Creates agent w/ config, agent needs to handle multiple instances of itself
    void* (*create_agent)(const char* config_json);
    void (*destroy_agent)(void* agent);

    // on game start
    void (*game_start)(void* agent, uint32_t agent_index, uint64_t seed); 
    Action (*agent_turn)(void* agent, const GameStateView* state);
    Action (*auction)(void* agent, const GameStateView* state, const AuctionView* auction);
    // Trade offer proposed elsewhere to this agent
    Action (*trade_offer)(void* agent, const GameStateView* state, const TradeOffer* offer);
} AgentVTable;

typedef struct {
    AgentVTable vtable;
} AgentExport;

AGENT_API AgentExport create_agent(const char* config_json);

#ifdef __cplusplus
}
#endif