#pragma once

#include <functional>
#include <string>
#include <vector>

namespace loop_rigger::audio {

using AudioBlock = std::vector<std::vector<float>>;

struct OfflineProcessingRequest {
    int channels = 2;
    int blockSize = 512;
    int blocks = 4;
    float inputValue = 0.25f;
    float expectedGain = 0.5f;
    float tolerance = 0.0005f;
};

struct OfflineProcessingResult {
    bool processed = false;
    int channels = 0;
    int blockSize = 0;
    int blocks = 0;
    float inputPeak = 0.0f;
    float expectedPeak = 0.0f;
    float outputPeak = 0.0f;
    float outputRms = 0.0f;
    float peakDelta = 0.0f;
    bool withinTolerance = false;
    std::string errorMessage;
};

using OfflineBlockProcessor = std::function<bool(AudioBlock& block, std::string& errorMessage)>;

float peakMagnitude(const AudioBlock& block);
float rmsMagnitude(const AudioBlock& block);
OfflineProcessingResult runOfflineProcessingSmoke(
    const OfflineProcessingRequest& request,
    const OfflineBlockProcessor& processor);

} // namespace loop_rigger::audio
