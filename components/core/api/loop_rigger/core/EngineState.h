#pragma once

#include <array>
#include <string>
#include <vector>

namespace loop_rigger::core {

constexpr int kInputPresetPages = 4;
constexpr int kInputPresetsPerPage = 8;
constexpr int kInputFxParameters = 8;
constexpr int kLoopers = 4;
constexpr int kTracksPerLooper = 4;
constexpr int kCenterFxSlots = 10;
constexpr int kCenterFxBanks = 5;
constexpr int kCenterFxParameters = 4;
constexpr int kCenterFxJoysticks = 2;
constexpr int kSamplerSlots = 8;
constexpr int kMasterParameters = 8;

enum class TrackState {
    Empty,
    Recording,
    Playing,
    Resampling
};

enum class ResampleMode {
    Off,
    SelectedLooper,
    AllLoopers
};

enum class RoutingSource {
    Mic,
    Synth,
    SelectedLooper,
    Looper1,
    Looper2,
    Looper3,
    Looper4,
    AllLoopers,
    RecordingBus
};

enum class RoutingTarget {
    SelectedTrack,
    SelectedLooper,
    Sampler,
    Master
};

struct InputControllerState {
    int selectedPage = 0;
    int selectedPreset = 0;
    float volume = 0.8F;
    float fxLevel = 0.8F;
    std::array<float, kInputFxParameters> fxParameters{};
};

struct TrackStateData {
    TrackState state = TrackState::Empty;
    int sampleLengthBeats = 4;
    float volume = 0.8F;
    float pan = 0.5F;
    bool selected = false;
};

struct LooperState {
    float volume = 0.8F;
    std::array<TrackStateData, kTracksPerLooper> tracks{};
};

struct TransportState {
    bool playing = false;
    int restartGeneration = 0;
};

struct RoutingState {
    RoutingSource source = RoutingSource::SelectedLooper;
    RoutingTarget target = RoutingTarget::SelectedTrack;
};

struct CenterFxState {
    int selectedSlot = 0;
    int selectedBank = 0;
    std::array<float, kCenterFxParameters> parameters{};
    std::array<float, kCenterFxJoysticks> joysticks{};
};

struct SamplerSlotState {
    bool loaded = false;
    bool playing = false;
};

struct SamplerState {
    std::array<SamplerSlotState, kSamplerSlots> slots{};
};

struct MasterState {
    std::array<float, kMasterParameters> parameters{};
};

struct EngineState {
    InputControllerState mic;
    InputControllerState synth;
    std::array<LooperState, kLoopers> loopers{};
    TransportState transport;
    RoutingState routing;
    CenterFxState centerFx;
    SamplerState sampler;
    MasterState master;
    int selectedLooper = 0;
    int selectedSampleLengthBeats = 4;
    ResampleMode resampleMode = ResampleMode::Off;
    std::vector<std::string> eventLog;
};

} // namespace loop_rigger::core
