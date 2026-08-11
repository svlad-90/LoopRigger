#pragma once

#include <cstdint>

namespace livelooping::core {

enum class ControllerId {
    MicKaossPad,
    SynthKaossPad,
    Yaeltex,
    PseudoGui
};

enum class InputTarget {
    Mic,
    Synth
};

enum class CommandType {
    SelectInputPresetPage,
    SelectInputPreset,
    SetInputVolume,
    SetInputFxLevel,
    SetInputFxParameter,
    SelectLooper,
    SelectSampleLength,
    ToggleTrackRecording,
    ClearTrack,
    SetTrackVolume,
    SetTrackPan,
    SetLooperVolume,
    ToggleTrackSelection,
    StartResampleSelectedLooper,
    StartResampleAllLoopers,
    StopResampling,
    ResetLooper,
    ResetAll
};

struct ControllerCommand {
    ControllerId controller = ControllerId::PseudoGui;
    CommandType type = CommandType::ResetAll;
    InputTarget inputTarget = InputTarget::Mic;
    int index = 0;
    int secondaryIndex = 0;
    float value = 0.0F;
};

} // namespace livelooping::core

