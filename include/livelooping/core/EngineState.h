#pragma once

#include <array>
#include <string>
#include <vector>

namespace livelooping::core {

constexpr int kInputPresetPages = 4;
constexpr int kInputPresetsPerPage = 8;
constexpr int kInputFxParameters = 8;
constexpr int kLoopers = 4;
constexpr int kTracksPerLooper = 4;

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

struct EngineState {
    InputControllerState mic;
    InputControllerState synth;
    std::array<LooperState, kLoopers> loopers{};
    int selectedLooper = 0;
    int selectedSampleLengthBeats = 4;
    ResampleMode resampleMode = ResampleMode::Off;
    std::vector<std::string> eventLog;
};

} // namespace livelooping::core

