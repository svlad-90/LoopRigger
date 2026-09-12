#include "loop_rigger/audio/HostGraphSpec.h"

#include <algorithm>
#include <set>
#include <utility>

namespace loop_rigger::audio {

namespace {

constexpr int kDefaultChannels = 2;

void addNode(HostGraphSpec& spec, std::string id, std::string label, HostNodeKind kind)
{
    spec.nodes.push_back({std::move(id), std::move(label), kind, kDefaultChannels});
}

void addRoute(
    HostGraphSpec& spec,
    std::string sourceNodeId,
    std::string targetNodeId,
    HostRouteKind kind = HostRouteKind::Audio,
    float gain = 1.0F,
    bool enabledByDefault = true,
    bool switchableAtRuntime = false,
    bool latencyCompensated = false)
{
    const auto id = sourceNodeId + "_to_" + targetNodeId;
    spec.routes.push_back({
        id,
        std::move(sourceNodeId),
        std::move(targetNodeId),
        kind,
        gain,
        enabledByDefault,
        switchableAtRuntime,
        true,
        latencyCompensated,
    });
}

void addBuiltIn(
    HostGraphSpec& spec,
    std::string id,
    std::string nodeId,
    BuiltInProcessorKind kind,
    std::string label,
    int latencySamples = 0)
{
    spec.builtIns.push_back({std::move(id), std::move(nodeId), kind, std::move(label), latencySamples});
}

void addPluginSlot(
    HostGraphSpec& spec,
    std::string id,
    std::string chainId,
    std::string branchId,
    std::string nodeId,
    PluginSlotRole role,
    std::string label,
    int order,
    int latencySamples = 0)
{
    spec.pluginSlots.push_back({
        std::move(id),
        std::move(chainId),
        std::move(branchId),
        std::move(nodeId),
        role,
        std::move(label),
        order,
        true,
        latencySamples,
    });
}

void addPluginChain(
    HostGraphSpec& spec,
    std::string id,
    std::string nodeId,
    std::string label,
    int explicitLatencySamples = 0)
{
    const auto chainId = id;
    spec.pluginChains.push_back({chainId, nodeId, std::move(label), true, true, explicitLatencySamples});
    spec.pluginBranches.push_back({chainId + "_serial", chainId, PluginBranchKind::Serial, "Serial", 1.0F, 0});
    spec.pluginBranches.push_back({chainId + "_parallel_a", chainId, PluginBranchKind::ParallelReturn, "Parallel A", 0.0F, 0});
    spec.pluginBranches.push_back({chainId + "_parallel_b", chainId, PluginBranchKind::ParallelReturn, "Parallel B", 0.0F, 0});
}

void addDefaultPluginSlot(
    HostGraphSpec& spec,
    std::string chainId,
    std::string nodeId,
    PluginSlotRole role,
    std::string label,
    int latencySamples = 0)
{
    addPluginSlot(
        spec,
        chainId + "_slot_1",
        chainId,
        chainId + "_serial",
        std::move(nodeId),
        role,
        std::move(label),
        0,
        latencySamples);
}

std::string looperId(int looper)
{
    return "looper_" + std::to_string(looper + 1);
}

std::string trackId(int looper, int track)
{
    return looperId(looper) + "_track_" + std::to_string(track + 1);
}

bool containsNode(const std::set<std::string>& nodes, const std::string& nodeId)
{
    return nodes.find(nodeId) != nodes.end();
}

} // namespace

HostGraphSpec makeDefaultLiveLoopingGraphSpec()
{
    HostGraphSpec spec;
    spec.latency = {1, 512, true, true, true};

    addNode(spec, "mic_input", "Mic input", HostNodeKind::Input);
    addNode(spec, "synth_input", "Synth input", HostNodeKind::Input);
    addNode(spec, "recording_bus", "Recording bus", HostNodeKind::RecordingBus);
    addNode(spec, "selected_track_fx", "Selected track FX", HostNodeKind::PerformanceFx);
    addNode(spec, "drop_fx", "Drop FX", HostNodeKind::PerformanceFx);
    addNode(spec, "repeater", "Repeater", HostNodeKind::PerformanceFx);
    addNode(spec, "remixer", "Remixer", HostNodeKind::PerformanceFx);
    addNode(spec, "one_shot_sampler", "One-shot sampler", HostNodeKind::Sampler);
    addNode(spec, "voice_synth", "Voice synth", HostNodeKind::Instrument);
    addNode(spec, "loopers_all_without_leads", "Loopers all without leads", HostNodeKind::GroupBus);
    addNode(spec, "loopers_all", "Loopers all", HostNodeKind::GroupBus);
    addNode(spec, "sidechain_bus", "Sidechain bus", HostNodeKind::Sidechain);
    addNode(spec, "master_fx", "Master FX", HostNodeKind::Master);
    addNode(spec, "master", "Master", HostNodeKind::Master);

    for (int looper = 0; looper < core::kLoopers; ++looper) {
        addNode(spec, looperId(looper), "Looper " + std::to_string(looper + 1), HostNodeKind::Looper);
        for (int track = 0; track < core::kTracksPerLooper; ++track) {
            const auto nodeId = trackId(looper, track);
            addNode(spec, nodeId, "L" + std::to_string(looper + 1) + " T" + std::to_string(track + 1), HostNodeKind::LooperTrack);
            addRoute(spec, "recording_bus", nodeId, HostRouteKind::RecordSource, 0.8F, false, true, true);
            addRoute(spec, nodeId, looperId(looper));
            addRoute(spec, nodeId, "selected_track_fx", HostRouteKind::ResampleSource, 0.0F, false, true, true);

            addBuiltIn(spec, nodeId + "_recorder", nodeId, BuiltInProcessorKind::LooperRecorder, "Loop recorder");
            addBuiltIn(spec, nodeId + "_mute_invert", nodeId, BuiltInProcessorKind::TrackMuteInvert, "Mute/invert");
            addBuiltIn(spec, nodeId + "_filter", nodeId, BuiltInProcessorKind::TrackFilter, "Track HP/LP");
            addBuiltIn(spec, nodeId + "_gain_pan", nodeId, BuiltInProcessorKind::GainPan, "Track gain/pan");
        }

        addBuiltIn(spec, looperId(looper) + "_gain_pan", looperId(looper), BuiltInProcessorKind::GainPan, "Looper gain/pan");
        addRoute(spec, looperId(looper), looper == 3 ? "loopers_all" : "loopers_all_without_leads");
    }

    addRoute(spec, "mic_input", "recording_bus", HostRouteKind::RecordSource, 0.8F, true, true, true);
    addRoute(spec, "synth_input", "recording_bus", HostRouteKind::RecordSource, 0.8F, true, true, true);
    addRoute(spec, "mic_input", "sidechain_bus", HostRouteKind::SidechainControl);
    addRoute(spec, "synth_input", "sidechain_bus", HostRouteKind::SidechainControl);
    addRoute(spec, "looper_1", "sidechain_bus", HostRouteKind::SidechainControl, 1.0F, true, true, false);
    addRoute(spec, "voice_synth", "recording_bus", HostRouteKind::RecordSource, 0.8F, true, true, true);
    addRoute(spec, "loopers_all_without_leads", "loopers_all");
    addRoute(spec, "loopers_all", "drop_fx");
    addRoute(spec, "drop_fx", "repeater");
    addRoute(spec, "repeater", "remixer");
    addRoute(spec, "remixer", "master_fx", HostRouteKind::Audio, 0.8F);
    addRoute(spec, "one_shot_sampler", "master_fx", HostRouteKind::Audio, 0.8F);
    addRoute(spec, "one_shot_sampler", "recording_bus", HostRouteKind::RecordSource, 0.8F, false, true, true);
    addRoute(spec, "selected_track_fx", "recording_bus", HostRouteKind::ResampleSource, 0.8F, false, true, true);
    addRoute(spec, "drop_fx", "recording_bus", HostRouteKind::ResampleSource, 0.8F, false, true, true);
    addRoute(spec, "repeater", "recording_bus", HostRouteKind::ResampleSource, 0.8F, false, true, true);
    addRoute(spec, "remixer", "recording_bus", HostRouteKind::ResampleSource, 0.8F, false, true, true);
    addRoute(spec, "master_fx", "master", HostRouteKind::Audio, 0.8F);

    addBuiltIn(spec, "recording_bus_meter", "recording_bus", BuiltInProcessorKind::Meter, "Recording bus meter");
    addBuiltIn(spec, "sidechain_envelope", "sidechain_bus", BuiltInProcessorKind::SidechainEnvelope, "Sidechain envelope");
    addBuiltIn(spec, "sidechain_spectral_analyzer", "sidechain_bus", BuiltInProcessorKind::SpectralSidechainAnalyzer, "Spectral sidechain analyzer", 256);
    addBuiltIn(spec, "sidechain_ducker", "loopers_all", BuiltInProcessorKind::SidechainDucker, "Looper sidechain");
    addBuiltIn(spec, "spectral_sidechain_ducker", "loopers_all", BuiltInProcessorKind::SpectralSidechainDucker, "Spectral looper sidechain", 256);
    addBuiltIn(spec, "master_meter", "master", BuiltInProcessorKind::Meter, "Master meter");
    addBuiltIn(spec, "master_limiter", "master", BuiltInProcessorKind::SafetyLimiter, "Final safety limiter");

    addPluginChain(spec, "mic_character_chain", "mic_input", "Mic character chain");
    addPluginChain(spec, "synth_character_chain", "synth_input", "Synth character chain");
    addPluginChain(spec, "selected_track_fx_chain", "selected_track_fx", "Selected-track FX chain");
    addPluginChain(spec, "drop_fx_chain", "drop_fx", "Drop FX chain");
    addPluginChain(spec, "repeater_fx_chain", "repeater", "Repeater FX chain");
    addPluginChain(spec, "remixer_fx_chain", "remixer", "Remixer FX chain");
    addPluginChain(spec, "sidechain_processor_chain", "sidechain_bus", "Sidechain processor chain");
    addPluginChain(spec, "master_fx_chain", "master_fx", "Master FX chain");

    addDefaultPluginSlot(spec, "mic_character_chain", "mic_input", PluginSlotRole::InputCharacter, "Mic character");
    addDefaultPluginSlot(spec, "synth_character_chain", "synth_input", PluginSlotRole::InputCharacter, "Synth character");
    addDefaultPluginSlot(spec, "selected_track_fx_chain", "selected_track_fx", PluginSlotRole::CreativeFx, "Selected-track creative FX");
    addDefaultPluginSlot(spec, "drop_fx_chain", "drop_fx", PluginSlotRole::CreativeFx, "Drop FX");
    addDefaultPluginSlot(spec, "repeater_fx_chain", "repeater", PluginSlotRole::CreativeFx, "Repeater FX");
    addDefaultPluginSlot(spec, "remixer_fx_chain", "remixer", PluginSlotRole::CreativeFx, "Remixer FX");
    addDefaultPluginSlot(spec, "sidechain_processor_chain", "sidechain_bus", PluginSlotRole::SidechainProcessor, "Spectral sidechain processor", 512);
    addDefaultPluginSlot(spec, "master_fx_chain", "master_fx", PluginSlotRole::ReverbDelay, "Reverb/delay");
    addPluginSlot(
        spec,
        "master_fx_chain_slot_2",
        "master_fx_chain",
        "master_fx_chain_serial",
        "master_fx",
        PluginSlotRole::PitchFormant,
        "Pitch/formant",
        1,
        128);
    addPluginSlot(
        spec,
        "master_fx_chain_slot_3",
        "master_fx_chain",
        "master_fx_chain_parallel_a",
        "master_fx",
        PluginSlotRole::Distortion,
        "Parallel distortion",
        0,
        64);
    addPluginSlot(
        spec,
        "master_fx_chain_slot_4",
        "master_fx_chain",
        "master_fx_chain_serial",
        "master_fx",
        PluginSlotRole::MasterCharacter,
        "Master character",
        2,
        64);

    return spec;
}

std::vector<std::string> validateHostGraphSpec(const HostGraphSpec& spec)
{
    std::vector<std::string> errors;
    std::set<std::string> nodeIds;
    std::set<std::string> routeIds;
    std::set<std::string> chainIds;
    std::set<std::string> branchIds;

    for (const auto& node : spec.nodes) {
        if (node.id.empty()) {
            errors.push_back("node id must not be empty");
            continue;
        }
        if (!nodeIds.insert(node.id).second) {
            errors.push_back("duplicate node id: " + node.id);
        }
        if (node.channels <= 0) {
            errors.push_back("node channels must be positive: " + node.id);
        }
    }

    for (const auto& route : spec.routes) {
        if (route.id.empty()) {
            errors.push_back("route id must not be empty");
        } else if (!routeIds.insert(route.id).second) {
            errors.push_back("duplicate route id: " + route.id);
        }
        if (!containsNode(nodeIds, route.sourceNodeId)) {
            errors.push_back("route source is unknown: " + route.sourceNodeId);
        }
        if (!containsNode(nodeIds, route.targetNodeId)) {
            errors.push_back("route target is unknown: " + route.targetNodeId);
        }
        if (route.gain < 0.0F || route.gain > 1.0F) {
            errors.push_back("route gain outside normalized range: " + route.sourceNodeId + " -> " + route.targetNodeId);
        }
    }

    if (spec.latency.maximumInputMonitoringBlocks < 0) {
        errors.push_back("maximum input monitoring blocks must not be negative");
    }
    if (spec.latency.blockSizeForCompensation <= 0) {
        errors.push_back("latency compensation block size must be positive");
    }

    for (const auto& builtIn : spec.builtIns) {
        if (builtIn.id.empty()) {
            errors.push_back("built-in processor id must not be empty");
        }
        if (!containsNode(nodeIds, builtIn.nodeId)) {
            errors.push_back("built-in processor node is unknown: " + builtIn.nodeId);
        }
        if (builtIn.latencySamples < 0) {
            errors.push_back("built-in processor latency must not be negative: " + builtIn.id);
        }
    }

    for (const auto& chain : spec.pluginChains) {
        if (chain.id.empty()) {
            errors.push_back("plugin chain id must not be empty");
        } else if (!chainIds.insert(chain.id).second) {
            errors.push_back("duplicate plugin chain id: " + chain.id);
        }
        if (!containsNode(nodeIds, chain.nodeId)) {
            errors.push_back("plugin chain node is unknown: " + chain.nodeId);
        }
        if (!chain.supportsSerial && !chain.supportsParallel) {
            errors.push_back("plugin chain must support serial or parallel routing: " + chain.id);
        }
        if (chain.explicitLatencySamples < 0) {
            errors.push_back("plugin chain explicit latency must not be negative: " + chain.id);
        }
    }

    for (const auto& branch : spec.pluginBranches) {
        if (branch.id.empty()) {
            errors.push_back("plugin branch id must not be empty");
        } else if (!branchIds.insert(branch.id).second) {
            errors.push_back("duplicate plugin branch id: " + branch.id);
        }
        if (chainIds.find(branch.chainId) == chainIds.end()) {
            errors.push_back("plugin branch chain is unknown: " + branch.chainId);
        }
        if (branch.returnGain < 0.0F || branch.returnGain > 1.0F) {
            errors.push_back("plugin branch return gain outside normalized range: " + branch.id);
        }
        if (branch.explicitLatencySamples < 0) {
            errors.push_back("plugin branch explicit latency must not be negative: " + branch.id);
        }
    }

    for (const auto& slot : spec.pluginSlots) {
        if (slot.id.empty()) {
            errors.push_back("plugin slot id must not be empty");
        }
        if (chainIds.find(slot.chainId) == chainIds.end()) {
            errors.push_back("plugin slot chain is unknown: " + slot.chainId);
        }
        if (branchIds.find(slot.branchId) == branchIds.end()) {
            errors.push_back("plugin slot branch is unknown: " + slot.branchId);
        }
        if (!containsNode(nodeIds, slot.nodeId)) {
            errors.push_back("plugin slot node is unknown: " + slot.nodeId);
        }
        if (slot.latencySamples < 0) {
            errors.push_back("plugin slot latency must not be negative: " + slot.id);
        }
    }

    return errors;
}

const HostNodeSpec* findHostNode(const HostGraphSpec& spec, const std::string& nodeId)
{
    const auto found = std::find_if(
        spec.nodes.begin(),
        spec.nodes.end(),
        [&nodeId](const HostNodeSpec& node) {
            return node.id == nodeId;
        });
    return found == spec.nodes.end() ? nullptr : &(*found);
}

} // namespace loop_rigger::audio
