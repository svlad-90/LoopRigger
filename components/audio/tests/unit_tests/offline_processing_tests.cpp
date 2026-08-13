#include "loop_rigger/audio/OfflineProcessing.h"

#include <cmath>
#include <iostream>
#include <string>

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

} // namespace

int main()
{
    testMagnitudes();
    testOfflineSmokeProcessesBlocks();
    testOfflineSmokeReportsProcessorFailure();
    return failures == 0 ? 0 : 1;
}
