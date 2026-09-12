#pragma once

#include "loop_rigger/core/EngineState.h"

#include <string>
#include <vector>

namespace loop_rigger::audio {

enum class HostNodeKind {
    Input,
    RecordingBus,
    Looper,
    LooperTrack,
    GroupBus,
    PerformanceFx,
    Sidechain,
    Sampler,
    Instrument,
    Master,
};

enum class HostRouteKind {
    Audio,
    RecordSource,
    ResampleSource,
    SidechainControl,
};

enum class BuiltInProcessorKind {
    GainPan,
    TrackMuteInvert,
    TrackFilter,
    LooperRecorder,
    SidechainEnvelope,
    SpectralSidechainAnalyzer,
    SidechainDucker,
    SpectralSidechainDucker,
    Meter,
    SafetyLimiter,
};

enum class PluginSlotRole {
    InputCharacter,
    CreativeFx,
    ReverbDelay,
    PitchFormant,
    Distortion,
    SidechainProcessor,
    MasterCharacter,
};

enum class PluginBranchKind {
    Serial,
    ParallelReturn,
};

struct HostNodeSpec {
    std::string id;
    std::string label;
    HostNodeKind kind = HostNodeKind::GroupBus;
    int channels = 2;
};

struct HostRouteSpec {
    std::string id;
    std::string sourceNodeId;
    std::string targetNodeId;
    HostRouteKind kind = HostRouteKind::Audio;
    float gain = 1.0F;
    bool enabledByDefault = true;
    bool switchableAtRuntime = false;
    bool smoothChanges = true;
    bool latencyCompensated = false;
};

struct BuiltInProcessorSpec {
    std::string id;
    std::string nodeId;
    BuiltInProcessorKind kind = BuiltInProcessorKind::GainPan;
    std::string label;
    int latencySamples = 0;
};

struct PluginSlotSpec {
    std::string id;
    std::string chainId;
    std::string branchId;
    std::string nodeId;
    PluginSlotRole role = PluginSlotRole::CreativeFx;
    std::string label;
    int order = 0;
    bool optional = true;
    int latencySamples = 0;
};

struct PluginBranchSpec {
    std::string id;
    std::string chainId;
    PluginBranchKind kind = PluginBranchKind::Serial;
    std::string label;
    float returnGain = 1.0F;
    int explicitLatencySamples = 0;
};

struct PluginChainSpec {
    std::string id;
    std::string nodeId;
    std::string label;
    bool supportsSerial = true;
    bool supportsParallel = true;
    int explicitLatencySamples = 0;
};

struct HostLatencyPolicy {
    int maximumInputMonitoringBlocks = 1;
    int blockSizeForCompensation = 512;
    bool compensatePluginLatencyForResampling = true;
    bool compensatePluginLatencyForParallelBranches = true;
    bool smoothRuntimeRouteChanges = true;
};

struct HostGraphSpec {
    std::vector<HostNodeSpec> nodes;
    std::vector<HostRouteSpec> routes;
    std::vector<BuiltInProcessorSpec> builtIns;
    std::vector<PluginChainSpec> pluginChains;
    std::vector<PluginBranchSpec> pluginBranches;
    std::vector<PluginSlotSpec> pluginSlots;
    HostLatencyPolicy latency;
};

HostGraphSpec makeDefaultLiveLoopingGraphSpec();
std::vector<std::string> validateHostGraphSpec(const HostGraphSpec& spec);
const HostNodeSpec* findHostNode(const HostGraphSpec& spec, const std::string& nodeId);

} // namespace loop_rigger::audio
