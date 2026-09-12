#include "loop_rigger/audio/HostGraphRuntime.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <set>
#include <utility>

namespace loop_rigger::audio {

namespace {

std::map<std::string, std::size_t> indexNodes(const HostGraphSpec& spec)
{
    std::map<std::string, std::size_t> indices;
    for (std::size_t index = 0; index < spec.nodes.size(); ++index) {
        indices[spec.nodes[index].id] = index;
    }
    return indices;
}

std::map<std::string, std::size_t> indexChains(const HostGraphSpec& spec)
{
    std::map<std::string, std::size_t> indices;
    for (std::size_t index = 0; index < spec.pluginChains.size(); ++index) {
        indices[spec.pluginChains[index].id] = index;
    }
    return indices;
}

std::map<std::string, std::size_t> indexBranches(const HostGraphSpec& spec)
{
    std::map<std::string, std::size_t> indices;
    for (std::size_t index = 0; index < spec.pluginBranches.size(); ++index) {
        indices[spec.pluginBranches[index].id] = index;
    }
    return indices;
}

bool isFeedbackBoundaryRoute(const HostRouteSpec& route)
{
    return route.latencyCompensated && route.switchableAtRuntime && !route.enabledByDefault && route.targetNodeId == "recording_bus";
}

int latencyBlocksForSamples(int latencySamples, int blockSize)
{
    if (latencySamples <= 0) {
        return 1;
    }
    return std::max(1, static_cast<int>(std::ceil(static_cast<double>(latencySamples) / static_cast<double>(std::max(1, blockSize)))));
}

bool contributesToDefaultOrdering(const HostGraphSpec& spec, const RuntimeRouteState& route)
{
    if (!route.active) {
        return false;
    }
    if (route.feedbackBoundary) {
        return false;
    }
    return spec.routes[route.specIndex].kind != HostRouteKind::SidechainControl;
}

std::vector<std::size_t> buildProcessingOrder(const HostGraphSpec& spec, const std::vector<RuntimeRouteState>& routes)
{
    std::vector<std::vector<std::size_t>> outgoing(spec.nodes.size());
    std::vector<int> incoming(spec.nodes.size(), 0);

    for (const auto& route : routes) {
        if (!contributesToDefaultOrdering(spec, route)) {
            continue;
        }
        outgoing[route.sourceNodeIndex].push_back(route.targetNodeIndex);
        ++incoming[route.targetNodeIndex];
    }

    std::queue<std::size_t> ready;
    for (std::size_t index = 0; index < incoming.size(); ++index) {
        if (incoming[index] == 0) {
            ready.push(index);
        }
    }

    std::vector<std::size_t> order;
    while (!ready.empty()) {
        const auto node = ready.front();
        ready.pop();
        order.push_back(node);

        for (const auto target : outgoing[node]) {
            --incoming[target];
            if (incoming[target] == 0) {
                ready.push(target);
            }
        }
    }

    if (order.size() != spec.nodes.size()) {
        order.clear();
        for (std::size_t index = 0; index < spec.nodes.size(); ++index) {
            order.push_back(index);
        }
    }

    return order;
}

void sortBranchSlots(const HostGraphSpec& spec, RuntimePluginBranchState& branch)
{
    std::sort(
        branch.pluginSlotIndices.begin(),
        branch.pluginSlotIndices.end(),
        [&spec](std::size_t left, std::size_t right) {
            const auto& leftSlot = spec.pluginSlots[left];
            const auto& rightSlot = spec.pluginSlots[right];
            if (leftSlot.order == rightSlot.order) {
                return leftSlot.id < rightSlot.id;
            }
            return leftSlot.order < rightSlot.order;
        });
}

int calculateBranchLatency(const HostGraphSpec& spec, const RuntimePluginBranchState& branch)
{
    const auto& branchSpec = spec.pluginBranches[branch.specIndex];
    if (branchSpec.explicitLatencySamples > 0) {
        return branchSpec.explicitLatencySamples;
    }

    int latency = 0;
    for (const auto slotIndex : branch.pluginSlotIndices) {
        if (branchSpec.kind == PluginBranchKind::Serial) {
            latency += spec.pluginSlots[slotIndex].latencySamples;
        } else {
            latency = std::max(latency, spec.pluginSlots[slotIndex].latencySamples);
        }
    }
    return latency;
}

int calculateChainLatency(const HostGraphSpec& spec, const RuntimePluginChainState& chain, const std::vector<RuntimePluginBranchState>& branches)
{
    const auto& chainSpec = spec.pluginChains[chain.specIndex];
    if (chainSpec.explicitLatencySamples > 0) {
        return chainSpec.explicitLatencySamples;
    }

    int latency = 0;
    for (const auto branchIndex : chain.branchStateIndices) {
        latency = std::max(latency, branches[branchIndex].latencySamples);
    }
    return latency;
}

int calculateNodeLatency(const HostGraphRuntime& runtime, const RuntimeNodeState& node)
{
    int latency = 0;
    for (const auto builtInIndex : node.builtInProcessorIndices) {
        latency += runtime.spec.builtIns[builtInIndex].latencySamples;
    }
    for (const auto chainIndex : node.pluginChainIndices) {
        latency += runtime.pluginChains[chainIndex].latencySamples;
    }
    return latency;
}

int calculateRouteLatencySamples(const HostGraphRuntime& runtime, const RuntimeRouteState& route)
{
    if (!route.latencyCompensated) {
        return 0;
    }
    return runtime.nodes[route.sourceNodeIndex].latencySamples + runtime.nodes[route.targetNodeIndex].latencySamples;
}

template <typename Item>
bool applyLatencyOverride(std::vector<Item>& items, const std::string& id, int latencySamples)
{
    const auto found = std::find_if(
        items.begin(),
        items.end(),
        [&id](const Item& item) {
            return item.id == id;
        });
    if (found == items.end()) {
        return false;
    }

    found->latencySamples = latencySamples;
    return true;
}

bool applyBranchLatencyOverride(std::vector<PluginBranchSpec>& branches, const std::string& id, int latencySamples)
{
    const auto found = std::find_if(
        branches.begin(),
        branches.end(),
        [&id](const PluginBranchSpec& branch) {
            return branch.id == id;
        });
    if (found == branches.end()) {
        return false;
    }

    found->explicitLatencySamples = latencySamples;
    return true;
}

bool applyChainLatencyOverride(std::vector<PluginChainSpec>& chains, const std::string& id, int latencySamples)
{
    const auto found = std::find_if(
        chains.begin(),
        chains.end(),
        [&id](const PluginChainSpec& chain) {
            return chain.id == id;
        });
    if (found == chains.end()) {
        return false;
    }

    found->explicitLatencySamples = latencySamples;
    return true;
}

std::vector<std::string> applyLatencyOverrides(HostGraphSpec& spec, const HostGraphLatencyOverrides& overrides)
{
    std::vector<std::string> errors;
    for (const auto& overrideEntry : overrides.entries) {
        if (overrideEntry.id.empty()) {
            errors.push_back("latency override id must not be empty");
            continue;
        }
        if (overrideEntry.latencySamples < 0) {
            errors.push_back("latency override must not be negative: " + overrideEntry.id);
            continue;
        }

        bool applied = false;
        switch (overrideEntry.target) {
        case LatencyOverrideTarget::BuiltInProcessor:
            applied = applyLatencyOverride(spec.builtIns, overrideEntry.id, overrideEntry.latencySamples);
            break;
        case LatencyOverrideTarget::PluginSlot:
            applied = applyLatencyOverride(spec.pluginSlots, overrideEntry.id, overrideEntry.latencySamples);
            break;
        case LatencyOverrideTarget::PluginBranch:
            applied = applyBranchLatencyOverride(spec.pluginBranches, overrideEntry.id, overrideEntry.latencySamples);
            break;
        case LatencyOverrideTarget::PluginChain:
            applied = applyChainLatencyOverride(spec.pluginChains, overrideEntry.id, overrideEntry.latencySamples);
            break;
        }

        if (!applied) {
            errors.push_back("latency override target is unknown: " + overrideEntry.id);
        }
    }
    return errors;
}

} // namespace

HostGraphRuntime buildHostGraphRuntime(HostGraphSpec spec)
{
    return buildHostGraphRuntime(std::move(spec), {});
}

HostGraphRuntime buildHostGraphRuntime(HostGraphSpec spec, const HostGraphLatencyOverrides& latencyOverrides)
{
    HostGraphRuntime runtime;
    runtime.errors = applyLatencyOverrides(spec, latencyOverrides);
    const auto validationErrors = validateHostGraphSpec(spec);
    runtime.errors.insert(runtime.errors.end(), validationErrors.begin(), validationErrors.end());
    runtime.spec = std::move(spec);

    const auto nodeIndices = indexNodes(runtime.spec);
    const auto chainIndices = indexChains(runtime.spec);
    const auto branchIndices = indexBranches(runtime.spec);

    runtime.nodes.resize(runtime.spec.nodes.size());
    for (std::size_t index = 0; index < runtime.nodes.size(); ++index) {
        runtime.nodes[index].specIndex = index;
    }

    runtime.routes.reserve(runtime.spec.routes.size());
    for (std::size_t index = 0; index < runtime.spec.routes.size(); ++index) {
        const auto& route = runtime.spec.routes[index];
        const auto source = nodeIndices.find(route.sourceNodeId);
        const auto target = nodeIndices.find(route.targetNodeId);
        if (source == nodeIndices.end() || target == nodeIndices.end()) {
            continue;
        }

        runtime.routes.push_back({
            index,
            source->second,
            target->second,
            route.enabledByDefault,
            route.switchableAtRuntime,
            route.smoothChanges && runtime.spec.latency.smoothRuntimeRouteChanges,
            route.latencyCompensated,
            isFeedbackBoundaryRoute(route),
            0,
            0,
        });
        if (route.enabledByDefault) {
            runtime.defaultActiveRouteIndices.push_back(runtime.routes.size() - 1);
        }
    }

    for (std::size_t index = 0; index < runtime.spec.builtIns.size(); ++index) {
        const auto node = nodeIndices.find(runtime.spec.builtIns[index].nodeId);
        if (node != nodeIndices.end()) {
            runtime.nodes[node->second].builtInProcessorIndices.push_back(index);
        }
    }

    runtime.pluginChains.resize(runtime.spec.pluginChains.size());
    for (std::size_t index = 0; index < runtime.pluginChains.size(); ++index) {
        runtime.pluginChains[index].specIndex = index;

        const auto node = nodeIndices.find(runtime.spec.pluginChains[index].nodeId);
        if (node != nodeIndices.end()) {
            runtime.nodes[node->second].pluginChainIndices.push_back(index);
        }
    }

    runtime.pluginBranches.resize(runtime.spec.pluginBranches.size());
    for (std::size_t index = 0; index < runtime.pluginBranches.size(); ++index) {
        runtime.pluginBranches[index].specIndex = index;

        const auto chain = chainIndices.find(runtime.spec.pluginBranches[index].chainId);
        if (chain != chainIndices.end()) {
            runtime.pluginChains[chain->second].branchStateIndices.push_back(index);
        }
    }

    for (std::size_t index = 0; index < runtime.spec.pluginSlots.size(); ++index) {
        const auto branch = branchIndices.find(runtime.spec.pluginSlots[index].branchId);
        if (branch != branchIndices.end()) {
            runtime.pluginBranches[branch->second].pluginSlotIndices.push_back(index);
        }
    }

    for (auto& branch : runtime.pluginBranches) {
        sortBranchSlots(runtime.spec, branch);
        branch.latencySamples = calculateBranchLatency(runtime.spec, branch);
    }

    for (auto& chain : runtime.pluginChains) {
        chain.latencySamples = calculateChainLatency(runtime.spec, chain, runtime.pluginBranches);
    }

    for (auto& node : runtime.nodes) {
        node.latencySamples = calculateNodeLatency(runtime, node);
    }

    for (auto& route : runtime.routes) {
        route.latencySamples = calculateRouteLatencySamples(runtime, route);
        route.latencyBlocks = latencyBlocksForSamples(route.latencySamples, runtime.spec.latency.blockSizeForCompensation);
    }

    runtime.processingOrder = buildProcessingOrder(runtime.spec, runtime.routes);
    return runtime;
}

const RuntimeRouteState* findRuntimeRoute(const HostGraphRuntime& runtime, const std::string& routeId)
{
    const auto found = std::find_if(
        runtime.routes.begin(),
        runtime.routes.end(),
        [&runtime, &routeId](const RuntimeRouteState& route) {
            return runtime.spec.routes[route.specIndex].id == routeId;
        });
    return found == runtime.routes.end() ? nullptr : &(*found);
}

bool setRuntimeRouteActive(HostGraphRuntime& runtime, const std::string& routeId, bool active)
{
    const auto found = std::find_if(
        runtime.routes.begin(),
        runtime.routes.end(),
        [&runtime, &routeId](const RuntimeRouteState& route) {
            return runtime.spec.routes[route.specIndex].id == routeId;
        });
    if (found == runtime.routes.end() || (!found->switchableAtRuntime && found->active != active)) {
        return false;
    }

    found->active = active;
    runtime.processingOrder = buildProcessingOrder(runtime.spec, runtime.routes);
    return true;
}

} // namespace loop_rigger::audio
