#pragma once

#include "loop_rigger/audio/HostGraphRuntime.h"
#include "loop_rigger/audio/OfflineProcessing.h"

#include <cstddef>
#include <deque>
#include <string>
#include <vector>

namespace loop_rigger::audio {

struct HostGraphProcessingConfig {
    int channels = 2;
    int blockSize = 512;
};

struct HostGraphBuffers {
    HostGraphProcessingConfig config;
    std::vector<AudioBlock> nodeBuffers;
};

struct HostGraphProcessResult {
    int activeRoutesMixed = 0;
    int feedbackBoundaryRoutes = 0;
    int compensatedFeedbackRoutesMixed = 0;
    float masterPeak = 0.0F;
    float masterRms = 0.0F;
};

struct HostGraphFeedbackRouteState {
    std::size_t runtimeRouteIndex = 0;
    int delayBlocks = 1;
    std::deque<AudioBlock> delayedBlocks;
};

struct HostGraphFeedbackCompensationState {
    HostGraphProcessingConfig config;
    int delayBlocks = 1;
    std::vector<HostGraphFeedbackRouteState> routes;
};

HostGraphBuffers makeHostGraphBuffers(const HostGraphRuntime& runtime, HostGraphProcessingConfig config);
HostGraphFeedbackCompensationState makeHostGraphFeedbackCompensationState(
    const HostGraphRuntime& runtime,
    HostGraphProcessingConfig config,
    int delayBlocks);
HostGraphFeedbackCompensationState makeHostGraphFeedbackCompensationState(
    const HostGraphRuntime& runtime,
    HostGraphProcessingConfig config);
void clearHostGraphBuffers(HostGraphBuffers& buffers);
AudioBlock* findNodeBuffer(HostGraphBuffers& buffers, const HostGraphRuntime& runtime, const std::string& nodeId);
const AudioBlock* findNodeBuffer(const HostGraphBuffers& buffers, const HostGraphRuntime& runtime, const std::string& nodeId);
HostGraphProcessResult processHostGraphRuntime(const HostGraphRuntime& runtime, HostGraphBuffers& buffers);
HostGraphProcessResult processHostGraphRuntime(
    const HostGraphRuntime& runtime,
    HostGraphBuffers& buffers,
    HostGraphFeedbackCompensationState& compensation);

} // namespace loop_rigger::audio
