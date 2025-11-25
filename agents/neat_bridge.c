// AI-Generated NEAT bridge agent with Claude Sonnet 4.5 
#include "agent_abi.h"
#include "state_view.h"
#include <Python.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Bridge agent that calls Python NEAT implementation

typedef struct {
    uint32_t agent_index;
    uint64_t seed;
    PyObject* neat_module;
    PyObject* neat_instance;
    char name[64];
} NEATAgent;

static int python_initialized = 0;

// Helper: Convert GameStateView to Python dict
static PyObject* state_to_python(const GameStateView* state, uint32_t agent_index) {
    PyObject* py_state = PyDict_New();
    
    // Basic game info
    PyDict_SetItemString(py_state, "game_id", PyLong_FromUnsignedLong(state->game_id));
    PyDict_SetItemString(py_state, "houses_remaining", PyLong_FromUnsignedLong(state->houses_remaining));
    PyDict_SetItemString(py_state, "hotels_remaining", PyLong_FromUnsignedLong(state->hotels_remaining));
    PyDict_SetItemString(py_state, "owed", PyLong_FromUnsignedLong(state->owed));
    
    // Players array
    PyObject* players_list = PyList_New(state->players_remaining);
    for (uint32_t i = 0; i < state->players_remaining; i++) {
        const PlayerView* p = &state->players[i];
        PyObject* player = PyDict_New();
        
        PyDict_SetItemString(player, "player_index", PyLong_FromUnsignedLong(p->player_index));
        PyDict_SetItemString(player, "cash", PyLong_FromUnsignedLong(p->cash));
        PyDict_SetItemString(player, "position", PyLong_FromUnsignedLong(p->position));
        PyDict_SetItemString(player, "retired", PyBool_FromLong(p->retired));
        PyDict_SetItemString(player, "in_jail", PyBool_FromLong(p->in_jail));
        PyDict_SetItemString(player, "turns_in_jail", PyLong_FromUnsignedLong(p->turns_in_jail));
        PyDict_SetItemString(player, "jail_free_cards", PyLong_FromUnsignedLong(p->jail_free_cards));
        PyDict_SetItemString(player, "railroads_owned", PyLong_FromUnsignedLong(p->railroads_owned));
        PyDict_SetItemString(player, "utilities_owned", PyLong_FromUnsignedLong(p->utilities_owned));
        
        PyList_SetItem(players_list, i, player);
    }
    PyDict_SetItemString(py_state, "players", players_list);
    
    // Properties array
    PyObject* props_list = PyList_New(state->num_properties);
    for (uint32_t i = 0; i < state->num_properties; i++) {
        const PropertyView* prop = &state->properties[i];
        PyObject* property = PyDict_New();
        
        PyDict_SetItemString(property, "position", PyLong_FromUnsignedLong(prop->position));
        PyDict_SetItemString(property, "property_id", PyLong_FromUnsignedLong(prop->property_id));
        PyDict_SetItemString(property, "owner_index", PyLong_FromUnsignedLong(prop->owner_index));
        PyDict_SetItemString(property, "is_owned", PyBool_FromLong(prop->is_owned));
        PyDict_SetItemString(property, "mortgaged", PyBool_FromLong(prop->mortgaged));
        PyDict_SetItemString(property, "type", PyLong_FromLong(prop->type));
        PyDict_SetItemString(property, "colour_id", PyLong_FromUnsignedLong(prop->colour_id));
        PyDict_SetItemString(property, "houses", PyLong_FromUnsignedLong(prop->houses));
        PyDict_SetItemString(property, "hotel", PyBool_FromLong(prop->hotel));
        PyDict_SetItemString(property, "purchase_price", PyLong_FromLong(prop->purchase_price));
        PyDict_SetItemString(property, "current_rent", PyLong_FromLong(prop->current_rent));
        
        PyList_SetItem(props_list, i, property);
    }
    PyDict_SetItemString(py_state, "properties", props_list);
    PyDict_SetItemString(py_state, "agent_index", PyLong_FromUnsignedLong(agent_index));
    
    return py_state;
}

// Helper: Convert auction to Python dict
static PyObject* auction_to_python(const AuctionView* auction) {
    PyObject* py_auction = PyDict_New();
    PyDict_SetItemString(py_auction, "property_id", PyLong_FromUnsignedLong(auction->property_id));
    return py_auction;
}

// Helper: Convert trade offer to Python dict
static PyObject* trade_to_python(const TradeOffer* offer) {
    PyObject* py_offer = PyDict_New();
    PyDict_SetItemString(py_offer, "player_to_offer", PyLong_FromUnsignedLong(offer->player_to_offer));
    
    // offer_from
    PyObject* from_dict = PyDict_New();
    PyDict_SetItemString(from_dict, "cash", PyLong_FromUnsignedLong(offer->offer_from.cash));
    PyDict_SetItemString(from_dict, "jail_cards", PyLong_FromUnsignedLong(offer->offer_from.jail_cards));
    PyDict_SetItemString(py_offer, "offer_from", from_dict);
    
    // offer_to
    PyObject* to_dict = PyDict_New();
    PyDict_SetItemString(to_dict, "cash", PyLong_FromUnsignedLong(offer->offer_to.cash));
    PyDict_SetItemString(to_dict, "jail_cards", PyLong_FromUnsignedLong(offer->offer_to.jail_cards));
    PyDict_SetItemString(py_offer, "offer_to", to_dict);
    
    return py_offer;
}

int abi_version() {
    return ABI_VERSION;
}

void* create_agent_instance(const char* config_json) {
    if (!python_initialized) {
        Py_Initialize();
        python_initialized = 1;
        
        // Add current directory to Python path
        PyRun_SimpleString("import sys; sys.path.insert(0, './agents')");
    }
    
    NEATAgent* agent = (NEATAgent*)malloc(sizeof(NEATAgent));
    if (!agent) return NULL;
    
    agent->agent_index = 0;
    agent->seed = 0;
    strncpy(agent->name, "NEATAgent", sizeof(agent->name)-1);
    agent->name[sizeof(agent->name)-1] = '\0';
    
    // Import Python module
    agent->neat_module = PyImport_ImportModule("neat_agent");
    if (!agent->neat_module) {
        PyErr_Print();
        free(agent);
        return NULL;
    }
    
    // Get NEATAgent class
    PyObject* agent_class = PyObject_GetAttrString(agent->neat_module, "NEATAgent");
    if (!agent_class) {
        PyErr_Print();
        Py_DECREF(agent->neat_module);
        free(agent);
        return NULL;
    }
    
    // Create instance: NEATAgent(config_json)
    PyObject* args = PyTuple_Pack(1, PyUnicode_FromString(config_json));
    agent->neat_instance = PyObject_CallObject(agent_class, args);
    Py_DECREF(args);
    Py_DECREF(agent_class);
    
    if (!agent->neat_instance) {
        PyErr_Print();
        Py_DECREF(agent->neat_module);
        free(agent);
        return NULL;
    }
    
    return agent;
}

void destroy_agent(void* agent_ptr) {
    if (agent_ptr) {
        NEATAgent* agent = (NEATAgent*)agent_ptr;
        if (agent->neat_instance) Py_DECREF(agent->neat_instance);
        if (agent->neat_module) Py_DECREF(agent->neat_module);
        free(agent);
    }
}

void game_start(void* agent_ptr, uint32_t agent_index, uint64_t seed) {
    NEATAgent* agent = (NEATAgent*)agent_ptr;
    if (!agent) return;
    
    agent->agent_index = agent_index;
    agent->seed = seed;
    
    // Call Python: agent.game_start(agent_index, seed)
    PyObject* result = PyObject_CallMethod(agent->neat_instance, "game_start", "IK", 
                                          agent_index, seed);
    if (result) Py_DECREF(result);
    else PyErr_Print();
}

Action agent_turn(void* agent_ptr, const GameStateView* state) {
    NEATAgent* agent = (NEATAgent*)agent_ptr;
    Action action = {0};
    action.type = ACTION_END_TURN;
    
    if (!agent || !state) return action;
    
    PyObject* py_state = state_to_python(state, agent->agent_index);
    PyObject* result = PyObject_CallMethod(agent->neat_instance, "agent_turn", "O", py_state);
    Py_DECREF(py_state);
    
    if (!result) {
        PyErr_Print();
        return action;
    }
    
    // Parse result: {"action_type": int, "buying_property": bool, "auction_bid": int, ...}
    PyObject* action_type = PyDict_GetItemString(result, "action_type");
    if (action_type) {
        action.type = (ActionType)PyLong_AsLong(action_type);
        
        if (action.type == ACTION_LANDED_PROPERTY) {
            PyObject* buying = PyDict_GetItemString(result, "buying_property");
            action.buying_property = buying ? PyObject_IsTrue(buying) : false;
        } else if (action.type == ACTION_AUCTION_BID) {
            PyObject* bid = PyDict_GetItemString(result, "auction_bid");
            action.auction_bid = bid ? PyLong_AsUnsignedLong(bid) : 0;
        } else if (action.type == ACTION_MORTGAGE) {
            PyObject* prop = PyDict_GetItemString(result, "property_position");
            action.property_position = prop ? PyLong_AsUnsignedLong(prop) : 0;
        } else if (action.type == ACTION_TRADE_RESPONSE) {
            PyObject* response = PyDict_GetItemString(result, "trade_response");
            action.trade_response = response ? PyObject_IsTrue(response) : false;
        }
    }
    
    Py_DECREF(result);
    return action;
}

Action auction(void* agent_ptr, const GameStateView* state, const AuctionView* auction) {
    NEATAgent* agent = (NEATAgent*)agent_ptr;
    Action action = {0};
    action.type = ACTION_END_TURN;
    
    if (!agent || !state || !auction) return action;
    
    PyObject* py_state = state_to_python(state, agent->agent_index);
    PyObject* py_auction = auction_to_python(auction);
    PyObject* result = PyObject_CallMethod(agent->neat_instance, "auction", "OO", 
                                          py_state, py_auction);
    Py_DECREF(py_state);
    Py_DECREF(py_auction);
    
    if (!result) {
        PyErr_Print();
        return action;
    }
    
    PyObject* action_type = PyDict_GetItemString(result, "action_type");
    if (action_type) {
        action.type = (ActionType)PyLong_AsLong(action_type);
        if (action.type == ACTION_AUCTION_BID) {
            PyObject* bid = PyDict_GetItemString(result, "auction_bid");
            action.auction_bid = bid ? PyLong_AsUnsignedLong(bid) : 0;
        }
    }
    
    Py_DECREF(result);
    return action;
}

Action trade_offer(void* agent_ptr, const GameStateView* state, const TradeOffer* offer) {
    NEATAgent* agent = (NEATAgent*)agent_ptr;
    Action action = {0};
    action.type = ACTION_TRADE_RESPONSE;
    action.trade_response = false;
    
    if (!agent || !state || !offer) return action;
    
    PyObject* py_state = state_to_python(state, agent->agent_index);
    PyObject* py_offer = trade_to_python(offer);
    PyObject* result = PyObject_CallMethod(agent->neat_instance, "trade_offer", "OO",
                                          py_state, py_offer);
    Py_DECREF(py_state);
    Py_DECREF(py_offer);
    
    if (!result) {
        PyErr_Print();
        return action;
    }
    
    PyObject* response = PyDict_GetItemString(result, "trade_response");
    action.trade_response = response ? PyObject_IsTrue(response) : false;
    
    Py_DECREF(result);
    return action;
}

AgentVTable vtable = {
    .abi_version = abi_version,
    .create_agent = create_agent_instance,
    .destroy_agent = destroy_agent,
    .game_start = game_start,
    .agent_turn = agent_turn,
    .auction = auction,
    .trade_offer = trade_offer
};

AGENT_API AgentExport create_agent_export(const char* config_json) {
    AgentExport export;
    export.vtable = vtable;
    return export;
}