#pragma once

#include <string>
#include <memory>
#include "agent_abi.h"
#include "plugin_loader.h"


struct AgentSpec {
    std::string path;
    std::string config_json;
    std::string name;
};

// Wrapper in c++ for engine to call agents easier
class AgentAdapter {
public:
    // Constructor
    AgentAdapter(const AgentSpec& spec);
    // Destructor
    ~AgentAdapter();

    // non-copyable
    AgentAdapter(const AgentAdapter&) = delete;
    AgentAdapter& operator=(const AgentAdapter&) = delete;

    // movable
    AgentAdapter(AgentAdapter&& other) noexcept
        : name_(std::move(other.name_))
        , export_(other.export_)      // assume this is trivially copyable / ok to copy
        , self_(other.self_)
        , handle_(std::move(other.handle_))
    {
        other.self_ = nullptr;        // moved-from object won’t destroy the agent
    }

    AgentAdapter& operator=(AgentAdapter&& other) noexcept {
        if (this != &other) {
            // clean up current resource
            if (self_) {
                export_.vtable.destroy_agent(self_);
            }

            name_   = std::move(other.name_);
            export_ = other.export_;
            self_   = other.self_;
            handle_ = std::move(other.handle_);

            other.self_ = nullptr;
        }
        return *this;
    }

    void game_start(uint32_t agent_index, uint64_t seed);
    Action agent_turn(const GameStateView* state);
    Action auction(const GameStateView* state, const AuctionView* auction);
    Action trade_offer(const GameStateView* state, const TradeOffer* offer);

    const std::string& name() const { return name_; };
private:
    std::string name_;
    AgentExport export_ = {};
    void* self_ = nullptr;
    std::unique_ptr<PluginHandle> handle_;
};