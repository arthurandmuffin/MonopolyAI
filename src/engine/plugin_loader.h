#pragma once
#include <string>
#include <memory>
#include "agent_abi.h"

// Abstract class
class PluginHandle {
public:
    virtual ~PluginHandle() = default;
    virtual AgentExport get_export() = 0;
    virtual AgentExport make(const std::string& cfg) = 0;
};

std::unique_ptr<PluginHandle> LoadAgentLibrary(const std::string& path);