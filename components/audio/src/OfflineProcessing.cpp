#include "loop_rigger/audio/OfflineProcessing.h"

#include <algorithm>
#include <cmath>

namespace loop_rigger::audio {

namespace {

AudioBlock makeFilledBlock(int channels, int samples, float value)
{
    return AudioBlock(
        static_cast<std::size_t>(std::max(1, channels)),
        std::vector<float>(static_cast<std::size_t>(std::max(1, samples)), value));
}

} // namespace

float peakMagnitude(const AudioBlock& block)
{
    float peak = 0.0f;
    for (const auto& channel : block) {
        for (const auto sample : channel) {
            peak = std::max(peak, std::abs(sample));
        }
    }
    return peak;
}

float rmsMagnitude(const AudioBlock& block)
{
    double sum = 0.0;
    int count = 0;
    for (const auto& channel : block) {
        for (const auto sample : channel) {
            sum += static_cast<double>(sample) * static_cast<double>(sample);
            ++count;
        }
    }
    return count > 0 ? static_cast<float>(std::sqrt(sum / static_cast<double>(count))) : 0.0f;
}

OfflineProcessingResult runOfflineProcessingSmoke(
    const OfflineProcessingRequest& request,
    const OfflineBlockProcessor& processor)
{
    OfflineProcessingResult result;
    result.channels = std::max(1, request.channels);
    result.blockSize = std::max(1, request.blockSize);
    result.blocks = std::max(1, request.blocks);
    result.inputPeak = std::abs(request.inputValue);
    result.expectedPeak = std::abs(request.inputValue * request.expectedGain);

    if (!processor) {
        result.errorMessage = "offline block processor is not set";
        return result;
    }

    for (int blockIndex = 0; blockIndex < result.blocks; ++blockIndex) {
        auto block = makeFilledBlock(result.channels, result.blockSize, request.inputValue);
        std::string errorMessage;
        if (!processor(block, errorMessage)) {
            result.errorMessage = errorMessage.empty() ? "offline block processor failed" : errorMessage;
            return result;
        }

        result.outputPeak = peakMagnitude(block);
        result.outputRms = rmsMagnitude(block);
    }

    result.peakDelta = std::abs(result.outputPeak - result.expectedPeak);
    result.withinTolerance = result.peakDelta < request.tolerance;
    result.processed = true;
    return result;
}

} // namespace loop_rigger::audio
