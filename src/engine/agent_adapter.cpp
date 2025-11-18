#include "agent_abi.h"
#include "agent_adapter.h"
#include "plugin_loader.h"
#include <stdexcept>

AgentAdapter::AgentAdapter(const AgentSpec& spec) : name_ (spec.name) {
    handle_ = LoadAgentLibrary(spec.path);
    export_ = handle_->make(spec.config_json);
    self_ = export_.vtable.create_agent(spec.config_json.c_str());
    if (!self_) {
        throw std::runtime_error("Agent create() returned null");
    }
}

AgentAdapter::~AgentAdapter() {
    if (self_) {
        export_.vtable.destroy_agent(self_);
    }
}

void AgentAdapter::game_start(uint32_t agent_index, uint64_t seed) {
    export_.vtable.game_start(self_, agent_index, seed);
}

Action AgentAdapter::agent_turn(const GameStateView* state) {
    return export_.vtable.agent_turn(self_, state);
}

Action AgentAdapter::auction(const GameStateView* state, const AuctionView* auction) {
    return export_.vtable.auction(self_, state, auction);
}

Action AgentAdapter::trade_offer(const GameStateView* state, const TradeOffer* offer) {
    return export_.vtable.trade_offer(self_, state, offer);
}