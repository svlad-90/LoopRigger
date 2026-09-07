#pragma once

#include "loop_rigger/audio/OfflineProcessing.h"
#include "loop_rigger/core/EngineState.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace loop_rigger::audio {

enum class HostBus {
    Mic,
    Synth,
    Looper1,
    Looper2,
    Looper3,
    Looper4,
    RecordingBus,
    CenterFx,
    Sampler,
    Master,
    Count
};

constexpr std::size_t kHostBuses = static_cast<std::size_t>(HostBus::Count);

struct AudioHostConfig {
    double sampleRate = 44100.0;
    int blockSize = 512;
    int channels = 2;
};

struct HostRoute {
    HostBus source = HostBus::Mic;
    HostBus target = HostBus::Master;
    float gain = 1.0F;
};

struct AudioHostBuffers {
    std::array<AudioBlock, kHostBuses> buses{};
};

struct AudioHostState {
    AudioHostConfig config;
    bool prepared = false;
    std::vector<HostRoute> routes;
    std::array<float, kHostBuses> busGains{};
    int processedBlocks = 0;
    float masterPeak = 0.0F;
    float masterRms = 0.0F;
};

class AudioHost {
public:
    AudioHost();

    void prepare(AudioHostConfig config);
    const AudioHostState& state() const;

    void clearRoutes();
    void addRoute(HostRoute route);
    void setBusGain(HostBus bus, float gain);
    void syncFromEngine(const core::EngineState& engineState);

    void process(AudioHostBuffers& buffers);

private:
    AudioHostState state_;
};

AudioBlock makeSilentAudioBlock(int channels, int samples);
std::string toString(HostBus bus);

} // namespace loop_rigger::audio
