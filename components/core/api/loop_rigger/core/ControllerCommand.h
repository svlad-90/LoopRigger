#pragma once

#include <cstdint>

namespace loop_rigger::core {

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
    ToggleInputFxHold,
    SetInputFxParameter,
    SelectClockDivision,
    SelectLooper,
    SelectSampleLength,
    ToggleTrackRecording,
    ClearTrack,
    ToggleTrackMute,
    ToggleLooperMute,
    ToggleTrackInvert,
    ToggleLooperInvert,
    SetTrackVolume,
    SetTrackPan,
    SetLooperVolume,
    ToggleTrackSelection,
    StartTransport,
    StopTransport,
    RestartAllLoopers,
    SelectRoutingSource,
    SelectRoutingTarget,
    SelectCenterFxSlot,
    SelectCenterFxBank,
    SetCenterFxParameter,
    SetCenterFxJoystick,
    TriggerRemixerMacro,
    TriggerRemixerMode,
    TriggerAnimationSlot,
    TriggerPresetSlot,
    SetSidechainParameter,
    SetTopFxParameter,
    SetMasterParameter,
    TriggerSamplerSlot,
    ClearSamplerSlot,
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

} // namespace loop_rigger::core
