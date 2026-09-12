#include "loop_rigger/audio/HostGraphProcessor.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace loop_rigger::audio {

namespace {

AudioBlock makeBlock(int channels, int samples)
{
    return AudioBlock(
        static_cast<std::size_t>(std::max(1, channels)),
        std::vector<float>(static_cast<std::size_t>(std::max(1, samples)), 0.0F));
}

AudioBlock makeCompatibleBlock(const HostGraphBuffers& buffers)
{
    return makeBlock(buffers.config.channels, buffers.config.blockSize);
}

void resizeBlock(AudioBlock& block, int channels, int samples)
{
    block.resize(static_cast<std::size_t>(std::max(1, channels)));
    for (auto& channel : block) {
        channel.resize(static_cast<std::size_t>(std::max(1, samples)), 0.0F);
    }
}

void clearBlock(AudioBlock& block)
{
    for (auto& channel : block) {
        std::fill(channel.begin(), channel.end(), 0.0F);
    }
}

void mixBlock(const AudioBlock& source, AudioBlock& target, float gain)
{
    const auto channels = std::min(source.size(), target.size());
    for (std::size_t channel = 0; channel < channels; ++channel) {
        const auto samples = std::min(source[channel].size(), target[channel].size());
        for (std::size_t sample = 0; sample < samples; ++sample) {
            target[channel][sample] += source[channel][sample] * gain;
        }
    }
}

AudioBlock copyBlockWithGain(const AudioBlock& source, int channels, int samples, float gain)
{
    auto copy = makeBlock(channels, samples);
    mixBlock(source, copy, gain);
    return copy;
}

int latencyBlocksForSamples(int latencySamples, int blockSize)
{
    if (latencySamples <= 0) {
        return 1;
    }
    return std::max(1, static_cast<int>(std::ceil(static_cast<double>(latencySamples) / static_cast<double>(std::max(1, blockSize)))));
}

const HostNodeSpec* nodeSpec(const HostGraphRuntime& runtime, const std::string& nodeId)
{
    return findHostNode(runtime.spec, nodeId);
}

bool shouldClearBeforeMix(const HostGraphRuntime& runtime, std::size_t nodeIndex, const std::vector<bool>& routedTargets)
{
    const auto kind = runtime.spec.nodes[runtime.nodes[nodeIndex].specIndex].kind;
    return routedTargets[nodeIndex] && kind != HostNodeKind::Input && kind != HostNodeKind::Instrument && kind != HostNodeKind::Sampler;
}

bool shouldMixRoute(const RuntimeRouteState& route)
{
    return route.active && !route.feedbackBoundary;
}

bool shouldCompensateRoute(const RuntimeRouteState& route)
{
    return route.active && route.feedbackBoundary;
}

std::size_t nodeIndexForId(const HostGraphRuntime& runtime, const std::string& nodeId)
{
    for (std::size_t index = 0; index < runtime.spec.nodes.size(); ++index) {
        if (runtime.spec.nodes[index].id == nodeId) {
            return index;
        }
    }
    return runtime.spec.nodes.size();
}

} // namespace

HostGraphBuffers makeHostGraphBuffers(const HostGraphRuntime& runtime, HostGraphProcessingConfig config)
{
    config.channels = std::max(1, config.channels);
    config.blockSize = std::max(1, config.blockSize);

    HostGraphBuffers buffers;
    buffers.config = config;
    buffers.nodeBuffers.reserve(runtime.spec.nodes.size());
    for (const auto& node : runtime.spec.nodes) {
        buffers.nodeBuffers.push_back(makeBlock(std::max(1, node.channels), config.blockSize));
    }
    return buffers;
}

HostGraphFeedbackCompensationState makeHostGraphFeedbackCompensationState(
    const HostGraphRuntime& runtime,
    HostGraphProcessingConfig config,
    int delayBlocks)
{
    config.channels = std::max(1, config.channels);
    config.blockSize = std::max(1, config.blockSize);

    HostGraphFeedbackCompensationState compensation;
    compensation.config = config;
    compensation.delayBlocks = std::max(1, delayBlocks);

    for (std::size_t index = 0; index < runtime.routes.size(); ++index) {
        if (!runtime.routes[index].feedbackBoundary) {
            continue;
        }

        HostGraphFeedbackRouteState routeState;
        routeState.runtimeRouteIndex = index;
        routeState.delayBlocks = compensation.delayBlocks;
        for (int delay = 0; delay < routeState.delayBlocks; ++delay) {
            routeState.delayedBlocks.push_back(makeBlock(config.channels, config.blockSize));
        }
        compensation.routes.push_back(std::move(routeState));
    }

    return compensation;
}

HostGraphFeedbackCompensationState makeHostGraphFeedbackCompensationState(
    const HostGraphRuntime& runtime,
    HostGraphProcessingConfig config)
{
    config.channels = std::max(1, config.channels);
    config.blockSize = std::max(1, config.blockSize);

    HostGraphFeedbackCompensationState compensation;
    compensation.config = config;
    compensation.delayBlocks = 1;

    for (std::size_t index = 0; index < runtime.routes.size(); ++index) {
        if (!runtime.routes[index].feedbackBoundary) {
            continue;
        }

        HostGraphFeedbackRouteState routeState;
        routeState.runtimeRouteIndex = index;
        routeState.delayBlocks = latencyBlocksForSamples(runtime.routes[index].latencySamples, config.blockSize);
        for (int delay = 0; delay < routeState.delayBlocks; ++delay) {
            routeState.delayedBlocks.push_back(makeBlock(config.channels, config.blockSize));
        }
        compensation.routes.push_back(std::move(routeState));
    }

    return compensation;
}

void clearHostGraphBuffers(HostGraphBuffers& buffers)
{
    for (auto& block : buffers.nodeBuffers) {
        clearBlock(block);
    }
}

AudioBlock* findNodeBuffer(HostGraphBuffers& buffers, const HostGraphRuntime& runtime, const std::string& nodeId)
{
    if (nodeSpec(runtime, nodeId) == nullptr) {
        return nullptr;
    }

    const auto index = nodeIndexForId(runtime, nodeId);
    if (index >= buffers.nodeBuffers.size()) {
        return nullptr;
    }
    return &buffers.nodeBuffers[index];
}

const AudioBlock* findNodeBuffer(const HostGraphBuffers& buffers, const HostGraphRuntime& runtime, const std::string& nodeId)
{
    if (nodeSpec(runtime, nodeId) == nullptr) {
        return nullptr;
    }

    const auto index = nodeIndexForId(runtime, nodeId);
    if (index >= buffers.nodeBuffers.size()) {
        return nullptr;
    }
    return &buffers.nodeBuffers[index];
}

HostGraphProcessResult processHostGraphRuntime(const HostGraphRuntime& runtime, HostGraphBuffers& buffers)
{
    HostGraphProcessResult result;
    if (!runtime.errors.empty()) {
        return result;
    }

    if (buffers.nodeBuffers.size() != runtime.spec.nodes.size()) {
        buffers = makeHostGraphBuffers(runtime, buffers.config);
    }

    for (auto& block : buffers.nodeBuffers) {
        resizeBlock(block, buffers.config.channels, buffers.config.blockSize);
    }

    std::vector<bool> routedTargets(runtime.spec.nodes.size(), false);
    for (const auto& route : runtime.routes) {
        if (shouldMixRoute(route)) {
            routedTargets[route.targetNodeIndex] = true;
        }
    }

    for (const auto nodeIndex : runtime.processingOrder) {
        if (nodeIndex < buffers.nodeBuffers.size() && shouldClearBeforeMix(runtime, nodeIndex, routedTargets)) {
            clearBlock(buffers.nodeBuffers[nodeIndex]);
        }
    }

    for (const auto& route : runtime.routes) {
        if (route.active && route.feedbackBoundary) {
            ++result.feedbackBoundaryRoutes;
            continue;
        }
        if (!shouldMixRoute(route)) {
            continue;
        }

        const auto& routeSpec = runtime.spec.routes[route.specIndex];
        mixBlock(
            buffers.nodeBuffers[route.sourceNodeIndex],
            buffers.nodeBuffers[route.targetNodeIndex],
            routeSpec.gain);
        ++result.activeRoutesMixed;
    }

    const auto* master = findNodeBuffer(buffers, runtime, "master");
    if (master != nullptr) {
        result.masterPeak = peakMagnitude(*master);
        result.masterRms = rmsMagnitude(*master);
    }
    return result;
}

HostGraphProcessResult processHostGraphRuntime(
    const HostGraphRuntime& runtime,
    HostGraphBuffers& buffers,
    HostGraphFeedbackCompensationState& compensation)
{
    auto result = processHostGraphRuntime(runtime, buffers);
    if (!runtime.errors.empty()) {
        return result;
    }

    if (compensation.config.channels != buffers.config.channels
        || compensation.config.blockSize != buffers.config.blockSize
        || compensation.routes.empty()) {
        compensation = compensation.delayBlocks > 1
            ? makeHostGraphFeedbackCompensationState(runtime, buffers.config, compensation.delayBlocks)
            : makeHostGraphFeedbackCompensationState(runtime, buffers.config);
    }

    for (auto& routeState : compensation.routes) {
        if (routeState.runtimeRouteIndex >= runtime.routes.size()) {
            continue;
        }

        const auto& route = runtime.routes[routeState.runtimeRouteIndex];
        if (!shouldCompensateRoute(route)) {
            continue;
        }

        if (routeState.delayedBlocks.empty()) {
            routeState.delayedBlocks.push_back(makeCompatibleBlock(buffers));
        }

        mixBlock(routeState.delayedBlocks.front(), buffers.nodeBuffers[route.targetNodeIndex], 1.0F);
        routeState.delayedBlocks.pop_front();

        const auto& routeSpec = runtime.spec.routes[route.specIndex];
        routeState.delayedBlocks.push_back(copyBlockWithGain(
            buffers.nodeBuffers[route.sourceNodeIndex],
            buffers.config.channels,
            buffers.config.blockSize,
            routeSpec.gain));
        ++result.compensatedFeedbackRoutesMixed;
    }

    const auto* master = findNodeBuffer(buffers, runtime, "master");
    if (master != nullptr) {
        result.masterPeak = peakMagnitude(*master);
        result.masterRms = rmsMagnitude(*master);
    }
    return result;
}

} // namespace loop_rigger::audio
