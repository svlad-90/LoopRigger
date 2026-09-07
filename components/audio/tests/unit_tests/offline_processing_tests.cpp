#include "loop_rigger/audio/AudioHost.h"
#include "loop_rigger/audio/OfflineProcessing.h"

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
    return failures == 0 ? 0 : 1;
}
