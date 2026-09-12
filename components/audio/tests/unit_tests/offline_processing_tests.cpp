#include "loop_rigger/audio/AudioHost.h"
#include "loop_rigger/audio/HostGraphProcessor.h"
#include "loop_rigger/audio/HostGraphRuntime.h"
#include "loop_rigger/audio/HostGraphSpec.h"
#include "loop_rigger/audio/OfflineProcessing.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << "\n";
    }
}

bool nearlyEqual(float left, float right, float tolerance = 0.00001f)
{
    return std::abs(left - right) < tolerance;
}

std::size_t busIndex(loop_rigger::audio::HostBus bus)
{
    return static_cast<std::size_t>(bus);
}

loop_rigger::audio::AudioBlock stereoBlock(float value, int samples)
{
    return loop_rigger::audio::AudioBlock {
        std::vector<float>(static_cast<std::size_t>(samples), value),
        std::vector<float>(static_cast<std::size_t>(samples), value),
    };
}

void testMagnitudes()
{
    const loop_rigger::audio::AudioBlock block {
        {0.25f, -0.5f},
        {0.25f, -0.5f},
    };

    expect(nearlyEqual(loop_rigger::audio::peakMagnitude(block), 0.5f), "peak magnitude should use absolute samples");
    expect(nearlyEqual(loop_rigger::audio::rmsMagnitude(block), 0.3952847f), "rms should include all channels");
}

void testOfflineSmokeProcessesBlocks()
{
    loop_rigger::audio::OfflineProcessingRequest request;
    request.channels = 2;
    request.blockSize = 4;
    request.blocks = 3;
    request.inputValue = 0.25f;
    request.expectedGain = 0.5f;

    int calls = 0;
    const auto result = loop_rigger::audio::runOfflineProcessingSmoke(
        request,
        [&calls](loop_rigger::audio::AudioBlock& block, std::string&) {
            ++calls;
            for (auto& channel : block) {
                for (auto& sample : channel) {
                    sample *= 0.5f;
                }
            }
            return true;
        });

    expect(result.processed, "offline smoke should report processed result");
    expect(calls == request.blocks, "offline smoke should invoke every block");
    expect(result.channels == request.channels, "result should preserve channel count");
    expect(result.blockSize == request.blockSize, "result should preserve block size");
    expect(nearlyEqual(result.inputPeak, 0.25f), "input peak should match request");
    expect(nearlyEqual(result.expectedPeak, 0.125f), "expected peak should use gain");
    expect(nearlyEqual(result.outputPeak, 0.125f), "output peak should be measured after processing");
    expect(nearlyEqual(result.outputRms, 0.125f), "output rms should be measured after processing");
    expect(result.withinTolerance, "processed peak should be within tolerance");
}

void testOfflineSmokeReportsProcessorFailure()
{
    loop_rigger::audio::OfflineProcessingRequest request;
    const auto result = loop_rigger::audio::runOfflineProcessingSmoke(
        request,
        [](loop_rigger::audio::AudioBlock&, std::string& errorMessage) {
            errorMessage = "processor unavailable";
            return false;
        });

    expect(!result.processed, "failed processor should not report processed result");
    expect(result.errorMessage == "processor unavailable", "processor error should be preserved");
}

void testAudioHostClampsPrepareConfig()
{
    loop_rigger::audio::AudioHost host;
    host.prepare({0.0, 0, 0});

    expect(host.state().prepared, "audio host should mark prepared state");
    expect(nearlyEqual(static_cast<float>(host.state().config.sampleRate), 44100.0f), "audio host should clamp sample rate");
    expect(host.state().config.blockSize == 1, "audio host should clamp block size");
    expect(host.state().config.channels == 1, "audio host should clamp channel count");
}

void testAudioHostMixesManualRoute()
{
    loop_rigger::audio::AudioHost host;
    host.prepare({48000.0, 4, 2});
    host.addRoute({loop_rigger::audio::HostBus::Mic, loop_rigger::audio::HostBus::Master, 0.5F});

    loop_rigger::audio::AudioHostBuffers buffers;
    buffers.buses[busIndex(loop_rigger::audio::HostBus::Mic)] = stereoBlock(1.0F, 4);
    host.process(buffers);

    expect(nearlyEqual(host.state().masterPeak, 0.5F), "manual host route should apply route gain");
    expect(nearlyEqual(host.state().masterRms, 0.5F), "manual host route should update master rms");
    expect(host.state().processedBlocks == 1, "audio host should count processed blocks");
}

void testAudioHostSyncsMicRouteFromEngine()
{
    loop_rigger::core::EngineState engineState;
    engineState.routing.source = loop_rigger::core::RoutingSource::Mic;
    engineState.routing.target = loop_rigger::core::RoutingTarget::Master;
    engineState.mic.volume = 0.25F;

    loop_rigger::audio::AudioHost host;
    host.prepare({48000.0, 4, 2});
    host.syncFromEngine(engineState);

    loop_rigger::audio::AudioHostBuffers buffers;
    buffers.buses[busIndex(loop_rigger::audio::HostBus::Mic)] = stereoBlock(1.0F, 4);
    host.process(buffers);

    expect(host.state().routes.size() == 2, "engine sync should route source through center FX");
    expect(nearlyEqual(host.state().masterPeak, 0.25F), "engine mic route should apply mic level once");
}

void testAudioHostSyncsAllLoopersFromEngine()
{
    loop_rigger::core::EngineState engineState;
    engineState.routing.source = loop_rigger::core::RoutingSource::AllLoopers;
    engineState.routing.target = loop_rigger::core::RoutingTarget::Master;
    for (auto& looper : engineState.loopers) {
        looper.volume = 0.25F;
    }
    engineState.loopers[1].muted = true;

    loop_rigger::audio::AudioHost host;
    host.prepare({48000.0, 4, 2});
    host.syncFromEngine(engineState);

    loop_rigger::audio::AudioHostBuffers buffers;
    buffers.buses[busIndex(loop_rigger::audio::HostBus::Looper1)] = stereoBlock(1.0F, 4);
    buffers.buses[busIndex(loop_rigger::audio::HostBus::Looper2)] = stereoBlock(1.0F, 4);
    buffers.buses[busIndex(loop_rigger::audio::HostBus::Looper3)] = stereoBlock(1.0F, 4);
    buffers.buses[busIndex(loop_rigger::audio::HostBus::Looper4)] = stereoBlock(1.0F, 4);
    host.process(buffers);

    expect(host.state().routes.size() == 5, "all-loopers sync should add one route per looper plus target route");
    expect(nearlyEqual(host.state().masterPeak, 0.75F), "all-loopers sync should mix unmuted loopers once");
}

int countNodes(loop_rigger::audio::HostGraphSpec const& spec, loop_rigger::audio::HostNodeKind kind)
{
    return static_cast<int>(std::count_if(
        spec.nodes.begin(),
        spec.nodes.end(),
        [kind](const loop_rigger::audio::HostNodeSpec& node) {
            return node.kind == kind;
        }));
}

bool hasRoute(
    const loop_rigger::audio::HostGraphSpec& spec,
    const std::string& source,
    const std::string& target,
    loop_rigger::audio::HostRouteKind kind,
    bool switchableAtRuntime = false)
{
    return std::any_of(
        spec.routes.begin(),
        spec.routes.end(),
        [&source, &target, kind, switchableAtRuntime](const loop_rigger::audio::HostRouteSpec& route) {
            return route.sourceNodeId == source && route.targetNodeId == target && route.kind == kind
                && route.switchableAtRuntime == switchableAtRuntime;
        });
}

bool hasBuiltIn(
    const loop_rigger::audio::HostGraphSpec& spec,
    const std::string& nodeId,
    loop_rigger::audio::BuiltInProcessorKind kind)
{
    return std::any_of(
        spec.builtIns.begin(),
        spec.builtIns.end(),
        [&nodeId, kind](const loop_rigger::audio::BuiltInProcessorSpec& builtIn) {
            return builtIn.nodeId == nodeId && builtIn.kind == kind;
        });
}

bool hasPluginSlot(
    const loop_rigger::audio::HostGraphSpec& spec,
    const std::string& nodeId,
    loop_rigger::audio::PluginSlotRole role)
{
    return std::any_of(
        spec.pluginSlots.begin(),
        spec.pluginSlots.end(),
        [&nodeId, role](const loop_rigger::audio::PluginSlotSpec& slot) {
            return slot.nodeId == nodeId && slot.role == role;
        });
}

bool hasPluginBranch(
    const loop_rigger::audio::HostGraphSpec& spec,
    const std::string& chainId,
    loop_rigger::audio::PluginBranchKind kind)
{
    return std::any_of(
        spec.pluginBranches.begin(),
        spec.pluginBranches.end(),
        [&chainId, kind](const loop_rigger::audio::PluginBranchSpec& branch) {
            return branch.chainId == chainId && branch.kind == kind;
        });
}

const loop_rigger::audio::RuntimePluginChainState* runtimePluginChain(
    const loop_rigger::audio::HostGraphRuntime& runtime,
    const std::string& chainId)
{
    const auto found = std::find_if(
        runtime.pluginChains.begin(),
        runtime.pluginChains.end(),
        [&runtime, &chainId](const loop_rigger::audio::RuntimePluginChainState& chain) {
            return runtime.spec.pluginChains[chain.specIndex].id == chainId;
        });
    return found == runtime.pluginChains.end() ? nullptr : &(*found);
}

const loop_rigger::audio::RuntimePluginBranchState* runtimePluginBranch(
    const loop_rigger::audio::HostGraphRuntime& runtime,
    const std::string& branchId)
{
    const auto found = std::find_if(
        runtime.pluginBranches.begin(),
        runtime.pluginBranches.end(),
        [&runtime, &branchId](const loop_rigger::audio::RuntimePluginBranchState& branch) {
            return runtime.spec.pluginBranches[branch.specIndex].id == branchId;
        });
    return found == runtime.pluginBranches.end() ? nullptr : &(*found);
}

const loop_rigger::audio::RuntimeRouteState* runtimeRouteById(
    const loop_rigger::audio::HostGraphRuntime& runtime,
    const std::string& routeId)
{
    return loop_rigger::audio::findRuntimeRoute(runtime, routeId);
}

void testDefaultHostGraphSpecCapturesLiveLoopingTopology()
{
    const auto spec = loop_rigger::audio::makeDefaultLiveLoopingGraphSpec();
    const auto errors = loop_rigger::audio::validateHostGraphSpec(spec);

    expect(errors.empty(), "default host graph spec should validate");
    expect(countNodes(spec, loop_rigger::audio::HostNodeKind::Looper) == loop_rigger::core::kLoopers, "host graph should expose four loopers");
    expect(
        countNodes(spec, loop_rigger::audio::HostNodeKind::LooperTrack) == loop_rigger::core::kLoopers * loop_rigger::core::kTracksPerLooper,
        "host graph should expose four tracks per looper");
    expect(loop_rigger::audio::findHostNode(spec, "recording_bus") != nullptr, "host graph should include an explicit recording bus");
    expect(loop_rigger::audio::findHostNode(spec, "sidechain_bus") != nullptr, "host graph should include an explicit sidechain bus");
    expect(loop_rigger::audio::findHostNode(spec, "one_shot_sampler") != nullptr, "host graph should include a one-shot sampler");
    expect(loop_rigger::audio::findHostNode(spec, "voice_synth") != nullptr, "host graph should include a voice synth");
    expect(spec.latency.maximumInputMonitoringBlocks == 1, "host graph should constrain live input monitoring latency");
    expect(spec.latency.compensatePluginLatencyForResampling, "host graph should require resampling latency compensation");
    expect(spec.latency.smoothRuntimeRouteChanges, "host graph should require smoothed live route changes");
}

void testDefaultHostGraphSpecKeepsResamplingAndCreativeFxConfigurable()
{
    const auto spec = loop_rigger::audio::makeDefaultLiveLoopingGraphSpec();

    expect(
        hasRoute(spec, "looper_1_track_1", "selected_track_fx", loop_rigger::audio::HostRouteKind::ResampleSource, true),
        "tracks should be able to feed selected-track FX for resampling");
    expect(
        hasRoute(spec, "selected_track_fx", "recording_bus", loop_rigger::audio::HostRouteKind::ResampleSource, true),
        "selected-track FX should be able to feed the recording bus");
    expect(
        hasRoute(spec, "one_shot_sampler", "recording_bus", loop_rigger::audio::HostRouteKind::RecordSource, true),
        "one-shot sampler should be able to feed the recording bus");
    expect(
        hasRoute(spec, "drop_fx", "recording_bus", loop_rigger::audio::HostRouteKind::ResampleSource, true),
        "drop FX should be able to feed the recording bus");
    expect(
        hasRoute(spec, "repeater", "recording_bus", loop_rigger::audio::HostRouteKind::ResampleSource, true),
        "repeater should be able to feed the recording bus");
    expect(
        hasRoute(spec, "remixer", "recording_bus", loop_rigger::audio::HostRouteKind::ResampleSource, true),
        "remixer should be able to feed the recording bus");
    expect(
        hasBuiltIn(spec, "looper_1_track_1", loop_rigger::audio::BuiltInProcessorKind::LooperRecorder),
        "track recording should be built into the host");
    expect(
        hasBuiltIn(spec, "loopers_all", loop_rigger::audio::BuiltInProcessorKind::SidechainDucker),
        "sidechain ducking should be built into the host");
    expect(
        hasBuiltIn(spec, "sidechain_bus", loop_rigger::audio::BuiltInProcessorKind::SpectralSidechainAnalyzer),
        "spectral sidechain analysis should be a first-class host responsibility");
    expect(
        hasBuiltIn(spec, "loopers_all", loop_rigger::audio::BuiltInProcessorKind::SpectralSidechainDucker),
        "spectral sidechain ducking should be a first-class host responsibility");
    expect(
        hasRoute(spec, "looper_1", "sidechain_bus", loop_rigger::audio::HostRouteKind::SidechainControl, true),
        "looper 1 should be available as a live sidechain source");
    expect(
        hasBuiltIn(spec, "master", loop_rigger::audio::BuiltInProcessorKind::SafetyLimiter),
        "master safety limiting should be centralized");
    expect(
        hasPluginSlot(spec, "drop_fx", loop_rigger::audio::PluginSlotRole::CreativeFx),
        "drop FX should stay a configurable creative plugin slot");
    expect(
        hasPluginSlot(spec, "master_fx", loop_rigger::audio::PluginSlotRole::ReverbDelay),
        "reverb/delay should stay configurable instead of being hardcoded");
    expect(
        hasPluginSlot(spec, "sidechain_bus", loop_rigger::audio::PluginSlotRole::SidechainProcessor),
        "spectral sidechain should keep a configurable plugin processor slot");
    expect(
        hasPluginBranch(spec, "master_fx_chain", loop_rigger::audio::PluginBranchKind::Serial),
        "plugin chains should support ordered serial slots");
    expect(
        hasPluginBranch(spec, "master_fx_chain", loop_rigger::audio::PluginBranchKind::ParallelReturn),
        "plugin chains should support parallel return branches");
}

void testHostGraphRuntimeBuildsExecutableRouteState()
{
    auto runtime = loop_rigger::audio::buildHostGraphRuntime(loop_rigger::audio::makeDefaultLiveLoopingGraphSpec());

    expect(runtime.errors.empty(), "runtime builder should accept default host graph spec");
    expect(runtime.nodes.size() == runtime.spec.nodes.size(), "runtime builder should index every node");
    expect(runtime.routes.size() == runtime.spec.routes.size(), "runtime builder should index every route");
    expect(!runtime.defaultActiveRouteIndices.empty(), "runtime builder should keep default-active route list");
    expect(runtime.processingOrder.size() == runtime.spec.nodes.size(), "runtime builder should produce a node processing order");

    const auto* recordRoute = loop_rigger::audio::findRuntimeRoute(runtime, "one_shot_sampler_to_recording_bus");
    expect(recordRoute != nullptr, "runtime should expose one-shot sampler record route");
    expect(recordRoute != nullptr && !recordRoute->active, "one-shot sampler record route should start disabled");
    expect(recordRoute != nullptr && recordRoute->switchableAtRuntime, "one-shot sampler record route should be runtime-switchable");
    expect(recordRoute != nullptr && recordRoute->latencyCompensated, "one-shot sampler record route should request latency compensation");
    expect(recordRoute != nullptr && recordRoute->feedbackBoundary, "one-shot sampler record route should be a feedback boundary");

    expect(
        loop_rigger::audio::setRuntimeRouteActive(runtime, "one_shot_sampler_to_recording_bus", true),
        "runtime should enable a switchable record route");

    recordRoute = loop_rigger::audio::findRuntimeRoute(runtime, "one_shot_sampler_to_recording_bus");
    expect(recordRoute != nullptr && recordRoute->active, "enabled switchable record route should stay active");

    expect(
        !loop_rigger::audio::setRuntimeRouteActive(runtime, "loopers_all_to_drop_fx", false),
        "runtime should reject toggling a non-switchable route");
}

void testHostGraphRuntimeIndexesPluginChainsAndOrdersSlots()
{
    const auto runtime = loop_rigger::audio::buildHostGraphRuntime(loop_rigger::audio::makeDefaultLiveLoopingGraphSpec());

    const auto* masterChain = runtimePluginChain(runtime, "master_fx_chain");
    const auto* serialBranch = runtimePluginBranch(runtime, "master_fx_chain_serial");
    const auto* parallelBranch = runtimePluginBranch(runtime, "master_fx_chain_parallel_a");

    expect(masterChain != nullptr, "runtime should index master plugin chain");
    expect(masterChain != nullptr && masterChain->branchStateIndices.size() == 3, "runtime should connect all master chain branches");
    expect(serialBranch != nullptr, "runtime should index master serial branch");
    expect(serialBranch != nullptr && serialBranch->pluginSlotIndices.size() == 3, "runtime should attach serial slots to master branch");
    if (serialBranch != nullptr && serialBranch->pluginSlotIndices.size() == 3) {
        expect(
            runtime.spec.pluginSlots[serialBranch->pluginSlotIndices[0]].id == "master_fx_chain_slot_1",
            "runtime should sort serial slots by order");
        expect(
            runtime.spec.pluginSlots[serialBranch->pluginSlotIndices[1]].id == "master_fx_chain_slot_2",
            "runtime should keep second serial slot after first");
        expect(
            runtime.spec.pluginSlots[serialBranch->pluginSlotIndices[2]].id == "master_fx_chain_slot_4",
            "runtime should keep later serial slots after lower order slots");
    }

    expect(parallelBranch != nullptr, "runtime should index master parallel branch");
    expect(parallelBranch != nullptr && parallelBranch->pluginSlotIndices.size() == 1, "runtime should attach parallel return slot");
}

void testHostGraphProcessorMixesDefaultRuntimeRoutes()
{
    const auto runtime = loop_rigger::audio::buildHostGraphRuntime(loop_rigger::audio::makeDefaultLiveLoopingGraphSpec());
    auto buffers = loop_rigger::audio::makeHostGraphBuffers(runtime, {2, 4});
    auto* looper4Track1 = loop_rigger::audio::findNodeBuffer(buffers, runtime, "looper_4_track_1");

    expect(looper4Track1 != nullptr, "graph buffers should expose looper 4 track 1 buffer");
    if (looper4Track1 != nullptr) {
        *looper4Track1 = stereoBlock(1.0F, 4);
    }

    const auto result = loop_rigger::audio::processHostGraphRuntime(runtime, buffers);
    const auto* master = loop_rigger::audio::findNodeBuffer(buffers, runtime, "master");

    expect(master != nullptr, "graph buffers should expose master buffer");
    expect(result.activeRoutesMixed > 0, "graph processor should mix default-active routes");
    expect(result.feedbackBoundaryRoutes == 0, "graph processor should not see inactive feedback boundaries");
    expect(nearlyEqual(result.masterPeak, 0.64F), "default looper 4 route should reach master through group FX gains");
    expect(master != nullptr && nearlyEqual(loop_rigger::audio::peakMagnitude(*master), 0.64F), "master buffer should contain mixed graph output");
}

void testHostGraphProcessorSeparatesFeedbackBoundaryRoutes()
{
    auto runtime = loop_rigger::audio::buildHostGraphRuntime(loop_rigger::audio::makeDefaultLiveLoopingGraphSpec());
    auto buffers = loop_rigger::audio::makeHostGraphBuffers(runtime, {2, 4});
    auto* sampler = loop_rigger::audio::findNodeBuffer(buffers, runtime, "one_shot_sampler");

    expect(
        loop_rigger::audio::setRuntimeRouteActive(runtime, "one_shot_sampler_to_recording_bus", true),
        "test should enable one-shot sampler recording route");
    expect(sampler != nullptr, "graph buffers should expose one-shot sampler buffer");
    if (sampler != nullptr) {
        *sampler = stereoBlock(1.0F, 4);
    }

    const auto result = loop_rigger::audio::processHostGraphRuntime(runtime, buffers);
    const auto* recordingBus = loop_rigger::audio::findNodeBuffer(buffers, runtime, "recording_bus");

    expect(result.feedbackBoundaryRoutes == 1, "graph processor should count enabled feedback-boundary route separately");
    expect(recordingBus != nullptr, "graph buffers should expose recording bus buffer");
    expect(
        recordingBus != nullptr && nearlyEqual(loop_rigger::audio::peakMagnitude(*recordingBus), 0.0F),
        "feedback-boundary route should not be mixed without latency compensation");
    expect(nearlyEqual(result.masterPeak, 0.64F), "one-shot sampler should still play through the default master path");
}

void testHostGraphProcessorCompensatesFeedbackBoundaryRoutesWithDelay()
{
    auto runtime = loop_rigger::audio::buildHostGraphRuntime(loop_rigger::audio::makeDefaultLiveLoopingGraphSpec());
    auto buffers = loop_rigger::audio::makeHostGraphBuffers(runtime, {2, 4});
    auto compensation = loop_rigger::audio::makeHostGraphFeedbackCompensationState(runtime, {2, 4}, 1);

    expect(
        loop_rigger::audio::setRuntimeRouteActive(runtime, "one_shot_sampler_to_recording_bus", true),
        "test should enable one-shot sampler recording route");

    auto* sampler = loop_rigger::audio::findNodeBuffer(buffers, runtime, "one_shot_sampler");
    expect(sampler != nullptr, "graph buffers should expose one-shot sampler buffer");
    if (sampler != nullptr) {
        *sampler = stereoBlock(1.0F, 4);
    }

    auto result = loop_rigger::audio::processHostGraphRuntime(runtime, buffers, compensation);
    auto* recordingBus = loop_rigger::audio::findNodeBuffer(buffers, runtime, "recording_bus");

    expect(result.feedbackBoundaryRoutes == 1, "compensated graph processor should still count active feedback boundary routes");
    expect(result.compensatedFeedbackRoutesMixed == 1, "compensated graph processor should evaluate enabled feedback boundary route");
    expect(
        recordingBus != nullptr && nearlyEqual(loop_rigger::audio::peakMagnitude(*recordingBus), 0.0F),
        "first compensated block should output the initial silent delay block");

    loop_rigger::audio::clearHostGraphBuffers(buffers);
    result = loop_rigger::audio::processHostGraphRuntime(runtime, buffers, compensation);
    recordingBus = loop_rigger::audio::findNodeBuffer(buffers, runtime, "recording_bus");

    expect(result.compensatedFeedbackRoutesMixed == 1, "second compensated block should evaluate the same feedback route");
    expect(
        recordingBus != nullptr && nearlyEqual(loop_rigger::audio::peakMagnitude(*recordingBus), 0.8F),
        "second compensated block should deliver the delayed one-shot sampler signal to recording bus");
}

void testHostGraphRuntimeCalculatesPluginLatencyForResamplingRoutes()
{
    auto spec = loop_rigger::audio::makeDefaultLiveLoopingGraphSpec();
    for (auto& slot : spec.pluginSlots) {
        if (slot.id == "remixer_fx_chain_slot_1") {
            slot.latencySamples = 1024;
        }
    }
    spec.latency.blockSizeForCompensation = 256;

    auto runtime = loop_rigger::audio::buildHostGraphRuntime(spec);
    const auto* route = runtimeRouteById(runtime, "remixer_to_recording_bus");

    expect(runtime.errors.empty(), "runtime should accept graph spec with explicit plugin latency");
    expect(route != nullptr, "runtime should expose remixer recording route");
    expect(route != nullptr && route->latencySamples == 1024, "runtime should carry plugin-chain latency onto resampling route");
    expect(route != nullptr && route->latencyBlocks == 4, "runtime should convert route latency samples to compensation blocks");
}

void testHostGraphProcessorBuildsAutoCompensationDelayFromRouteLatency()
{
    auto spec = loop_rigger::audio::makeDefaultLiveLoopingGraphSpec();
    for (auto& slot : spec.pluginSlots) {
        if (slot.id == "remixer_fx_chain_slot_1") {
            slot.latencySamples = 1024;
        }
    }

    auto runtime = loop_rigger::audio::buildHostGraphRuntime(spec);
    const auto compensation = loop_rigger::audio::makeHostGraphFeedbackCompensationState(runtime, {2, 256});

    const auto found = std::find_if(
        compensation.routes.begin(),
        compensation.routes.end(),
        [&runtime](const loop_rigger::audio::HostGraphFeedbackRouteState& routeState) {
            const auto routeIndex = routeState.runtimeRouteIndex;
            return routeIndex < runtime.routes.size()
                && runtime.spec.routes[runtime.routes[routeIndex].specIndex].id == "remixer_to_recording_bus";
        });

    expect(found != compensation.routes.end(), "auto compensation should include remixer recording route");
    expect(found != compensation.routes.end() && found->delayBlocks == 4, "auto compensation should derive delay blocks from route latency");
    expect(found != compensation.routes.end() && static_cast<int>(found->delayedBlocks.size()) == 4, "auto compensation should prefill route delay queue");
}

void testHostGraphRuntimeAppliesLatencyOverrides()
{
    loop_rigger::audio::HostGraphLatencyOverrides overrides;
    overrides.entries.push_back({
        loop_rigger::audio::LatencyOverrideTarget::PluginSlot,
        "remixer_fx_chain_slot_1",
        768,
    });

    auto spec = loop_rigger::audio::makeDefaultLiveLoopingGraphSpec();
    spec.latency.blockSizeForCompensation = 256;

    const auto runtime = loop_rigger::audio::buildHostGraphRuntime(spec, overrides);
    const auto* route = runtimeRouteById(runtime, "remixer_to_recording_bus");

    expect(runtime.errors.empty(), "runtime should accept known latency override");
    expect(route != nullptr, "runtime should expose remixer recording route after latency override");
    expect(route != nullptr && route->latencySamples == 768, "plugin slot latency override should propagate to route latency");
    expect(route != nullptr && route->latencyBlocks == 3, "plugin slot latency override should update route compensation blocks");
}

void testHostGraphRuntimeAppliesChainLatencyOverride()
{
    loop_rigger::audio::HostGraphLatencyOverrides overrides;
    overrides.entries.push_back({
        loop_rigger::audio::LatencyOverrideTarget::PluginChain,
        "remixer_fx_chain",
        2048,
    });

    auto spec = loop_rigger::audio::makeDefaultLiveLoopingGraphSpec();
    spec.latency.blockSizeForCompensation = 512;

    const auto runtime = loop_rigger::audio::buildHostGraphRuntime(spec, overrides);
    const auto* route = runtimeRouteById(runtime, "remixer_to_recording_bus");

    expect(runtime.errors.empty(), "runtime should accept known chain latency override");
    expect(route != nullptr, "runtime should expose remixer recording route after chain latency override");
    expect(route != nullptr && route->latencySamples == 2048, "plugin chain latency override should propagate to route latency");
    expect(route != nullptr && route->latencyBlocks == 4, "plugin chain latency override should update route compensation blocks");
}

void testHostGraphRuntimeRejectsInvalidLatencyOverrides()
{
    loop_rigger::audio::HostGraphLatencyOverrides overrides;
    overrides.entries.push_back({
        loop_rigger::audio::LatencyOverrideTarget::PluginSlot,
        "missing_slot",
        128,
    });
    overrides.entries.push_back({
        loop_rigger::audio::LatencyOverrideTarget::PluginSlot,
        "remixer_fx_chain_slot_1",
        -1,
    });

    const auto runtime = loop_rigger::audio::buildHostGraphRuntime(
        loop_rigger::audio::makeDefaultLiveLoopingGraphSpec(),
        overrides);

    expect(runtime.errors.size() == 2, "runtime should report unknown and negative latency overrides");
}

} // namespace

int main()
{
    testMagnitudes();
    testOfflineSmokeProcessesBlocks();
    testOfflineSmokeReportsProcessorFailure();
    testAudioHostClampsPrepareConfig();
    testAudioHostMixesManualRoute();
    testAudioHostSyncsMicRouteFromEngine();
    testAudioHostSyncsAllLoopersFromEngine();
    testDefaultHostGraphSpecCapturesLiveLoopingTopology();
    testDefaultHostGraphSpecKeepsResamplingAndCreativeFxConfigurable();
    testHostGraphRuntimeBuildsExecutableRouteState();
    testHostGraphRuntimeIndexesPluginChainsAndOrdersSlots();
    testHostGraphProcessorMixesDefaultRuntimeRoutes();
    testHostGraphProcessorSeparatesFeedbackBoundaryRoutes();
    testHostGraphProcessorCompensatesFeedbackBoundaryRoutesWithDelay();
    testHostGraphRuntimeCalculatesPluginLatencyForResamplingRoutes();
    testHostGraphProcessorBuildsAutoCompensationDelayFromRouteLatency();
    testHostGraphRuntimeAppliesLatencyOverrides();
    testHostGraphRuntimeAppliesChainLatencyOverride();
    testHostGraphRuntimeRejectsInvalidLatencyOverrides();
    return failures == 0 ? 0 : 1;
}
