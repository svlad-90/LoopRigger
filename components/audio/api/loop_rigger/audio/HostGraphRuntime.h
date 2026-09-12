#pragma once

#include "loop_rigger/audio/HostGraphSpec.h"

#include <cstddef>
#include <string>
#include <vector>

namespace loop_rigger::audio {

enum class LatencyOverrideTarget {
    BuiltInProcessor,
    PluginSlot,
    PluginBranch,
    PluginChain,
};

struct HostGraphLatencyOverride {
    LatencyOverrideTarget target = LatencyOverrideTarget::PluginSlot;
    std::string id;
    int latencySamples = 0;
};

struct HostGraphLatencyOverrides {
    std::vector<HostGraphLatencyOverride> entries;
};

struct RuntimeRouteState {
    std::size_t specIndex = 0;
    std::size_t sourceNodeIndex = 0;
    std::size_t targetNodeIndex = 0;
    bool active = false;
    bool switchableAtRuntime = false;
    bool smoothChanges = true;
    bool latencyCompensated = false;
    bool feedbackBoundary = false;
    int latencySamples = 0;
    int latencyBlocks = 0;
};

struct RuntimeNodeState {
    std::size_t specIndex = 0;
    int latencySamples = 0;
    std::vector<std::size_t> builtInProcessorIndices;
    std::vector<std::size_t> pluginChainIndices;
};

struct RuntimePluginBranchState {
    std::size_t specIndex = 0;
    int latencySamples = 0;
    std::vector<std::size_t> pluginSlotIndices;
};

struct RuntimePluginChainState {
    std::size_t specIndex = 0;
    int latencySamples = 0;
    std::vector<std::size_t> branchStateIndices;
};

struct HostGraphRuntime {
    HostGraphSpec spec;
    std::vector<RuntimeNodeState> nodes;
    std::vector<RuntimeRouteState> routes;
    std::vector<RuntimePluginChainState> pluginChains;
    std::vector<RuntimePluginBranchState> pluginBranches;
    std::vector<std::size_t> defaultActiveRouteIndices;
    std::vector<std::size_t> processingOrder;
    std::vector<std::string> errors;
};

HostGraphRuntime buildHostGraphRuntime(HostGraphSpec spec);
HostGraphRuntime buildHostGraphRuntime(HostGraphSpec spec, const HostGraphLatencyOverrides& latencyOverrides);
const RuntimeRouteState* findRuntimeRoute(const HostGraphRuntime& runtime, const std::string& routeId);
bool setRuntimeRouteActive(HostGraphRuntime& runtime, const std::string& routeId, bool active);

} // namespace loop_rigger::audio
