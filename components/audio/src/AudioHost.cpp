#include "loop_rigger/audio/AudioHost.h"

#include <algorithm>
#include <stdexcept>

namespace loop_rigger::audio {

namespace {

std::size_t busIndex(HostBus bus)
{
    return static_cast<std::size_t>(bus);
}

float clampGain(float gain)
{
    return std::clamp(gain, 0.0F, 1.0F);
}

HostBus busForLooper(int looperIndex)
{
    switch (looperIndex) {
    case 0:
        return HostBus::Looper1;
    case 1:
        return HostBus::Looper2;
    case 2:
        return HostBus::Looper3;
    case 3:
        return HostBus::Looper4;
    default:
        throw std::out_of_range("looper index outside host bus range");
    }
}

HostBus sourceBus(core::RoutingSource source, int selectedLooper)
{
    switch (source) {
    case core::RoutingSource::Mic:
        return HostBus::Mic;
    case core::RoutingSource::Synth:
        return HostBus::Synth;
    case core::RoutingSource::SelectedLooper:
        return busForLooper(selectedLooper);
    case core::RoutingSource::Looper1:
        return HostBus::Looper1;
    case core::RoutingSource::Looper2:
        return HostBus::Looper2;
    case core::RoutingSource::Looper3:
        return HostBus::Looper3;
    case core::RoutingSource::Looper4:
        return HostBus::Looper4;
    case core::RoutingSource::AllLoopers:
        return HostBus::CenterFx;
    case core::RoutingSource::RecordingBus:
        return HostBus::RecordingBus;
    }
    return HostBus::Mic;
}

HostBus targetBus(core::RoutingTarget target)
{
    switch (target) {
    case core::RoutingTarget::SelectedTrack:
        return HostBus::RecordingBus;
    case core::RoutingTarget::SelectedLooper:
        return HostBus::RecordingBus;
    case core::RoutingTarget::Sampler:
        return HostBus::Sampler;
    case core::RoutingTarget::Master:
        return HostBus::Master;
    }
    return HostBus::Master;
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

} // namespace

AudioHost::AudioHost()
{
    state_.busGains.fill(1.0F);
}

void AudioHost::prepare(AudioHostConfig config)
{
    state_.config.sampleRate = config.sampleRate > 0.0 ? config.sampleRate : 44100.0;
    state_.config.blockSize = std::max(1, config.blockSize);
    state_.config.channels = std::max(1, config.channels);
    state_.prepared = true;
    state_.processedBlocks = 0;
    state_.masterPeak = 0.0F;
    state_.masterRms = 0.0F;
}

const AudioHostState& AudioHost::state() const
{
    return state_;
}

void AudioHost::clearRoutes()
{
    state_.routes.clear();
}

void AudioHost::addRoute(HostRoute route)
{
    route.gain = clampGain(route.gain);
    state_.routes.push_back(route);
}

void AudioHost::setBusGain(HostBus bus, float gain)
{
    state_.busGains[busIndex(bus)] = clampGain(gain);
}

void AudioHost::syncFromEngine(const core::EngineState& engineState)
{
    clearRoutes();

    if (engineState.routing.source == core::RoutingSource::AllLoopers) {
        for (int looper = 0; looper < core::kLoopers; ++looper) {
            addRoute({busForLooper(looper), HostBus::CenterFx, 1.0F});
        }
    } else {
        addRoute({sourceBus(engineState.routing.source, engineState.selectedLooper), HostBus::CenterFx, 1.0F});
    }

    addRoute({HostBus::CenterFx, targetBus(engineState.routing.target), 1.0F});

    for (int looper = 0; looper < core::kLoopers; ++looper) {
        setBusGain(busForLooper(looper), engineState.loopers[looper].muted ? 0.0F : engineState.loopers[looper].volume);
    }
    setBusGain(HostBus::Mic, engineState.mic.volume);
    setBusGain(HostBus::Synth, engineState.synth.volume);
    setBusGain(HostBus::Master, 1.0F);
}

void AudioHost::process(AudioHostBuffers& buffers)
{
    if (!state_.prepared) {
        prepare({});
    }

    for (auto& block : buffers.buses) {
        resizeBlock(block, state_.config.channels, state_.config.blockSize);
    }

    std::array<bool, kHostBuses> routeTargets{};
    routeTargets.fill(false);
    for (const auto& route : state_.routes) {
        routeTargets[busIndex(route.target)] = true;
    }

    for (std::size_t index = 0; index < buffers.buses.size(); ++index) {
        if (routeTargets[index]) {
            clearBlock(buffers.buses[index]);
        }
    }

    for (const auto& route : state_.routes) {
        const auto sourceIndex = busIndex(route.source);
        const auto targetIndex = busIndex(route.target);
        const auto sourceGain = state_.busGains[sourceIndex];
        const auto targetGain = state_.busGains[targetIndex];
        mixBlock(buffers.buses[sourceIndex], buffers.buses[targetIndex], route.gain * sourceGain * targetGain);
    }

    const auto& master = buffers.buses[busIndex(HostBus::Master)];
    state_.masterPeak = peakMagnitude(master);
    state_.masterRms = rmsMagnitude(master);
    ++state_.processedBlocks;
}

AudioBlock makeSilentAudioBlock(int channels, int samples)
{
    return AudioBlock(
        static_cast<std::size_t>(std::max(1, channels)),
        std::vector<float>(static_cast<std::size_t>(std::max(1, samples)), 0.0F));
}

std::string toString(HostBus bus)
{
    switch (bus) {
    case HostBus::Mic:
        return "mic";
    case HostBus::Synth:
        return "synth";
    case HostBus::Looper1:
        return "looper_1";
    case HostBus::Looper2:
        return "looper_2";
    case HostBus::Looper3:
        return "looper_3";
    case HostBus::Looper4:
        return "looper_4";
    case HostBus::RecordingBus:
        return "recording_bus";
    case HostBus::CenterFx:
        return "center_fx";
    case HostBus::Sampler:
        return "sampler";
    case HostBus::Master:
        return "master";
    case HostBus::Count:
        return "count";
    }
    return "unknown";
}

} // namespace loop_rigger::audio
