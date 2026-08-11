#include "livelooping/core/LiveLoopingEngine.h"

#include <cstdlib>
#include <iostream>
#include <string>

using livelooping::core::CommandType;
using livelooping::core::ControllerCommand;
using livelooping::core::ControllerId;
using livelooping::core::InputTarget;
using livelooping::core::LiveLoopingEngine;
using livelooping::core::ResampleMode;
using livelooping::core::TrackState;

namespace {

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << "\n";
    }
}

ControllerCommand inputCommand(InputTarget target, CommandType type, int index = 0, float value = 0.0F)
{
    ControllerCommand command;
    command.controller = target == InputTarget::Mic ? ControllerId::MicKaossPad : ControllerId::SynthKaossPad;
    command.inputTarget = target;
    command.type = type;
    command.index = index;
    command.value = value;
    return command;
}

ControllerCommand yaeltexCommand(CommandType type, int index = 0, float value = 0.0F)
{
    ControllerCommand command;
    command.controller = ControllerId::Yaeltex;
    command.type = type;
    command.index = index;
    command.value = value;
    return command;
}

void testInputControllerCommands()
{
    LiveLoopingEngine engine;

    engine.handle(inputCommand(InputTarget::Mic, CommandType::SelectInputPresetPage, 2));
    engine.handle(inputCommand(InputTarget::Mic, CommandType::SelectInputPreset, 5));
    engine.handle(inputCommand(InputTarget::Mic, CommandType::SetInputVolume, 0, 1.25F));
    engine.handle(inputCommand(InputTarget::Synth, CommandType::SelectInputPreset, 3));
    engine.handle(inputCommand(InputTarget::Synth, CommandType::SetInputFxParameter, 4, 0.42F));

    const auto& state = engine.state();
    expect(state.mic.selectedPage == 2, "mic page should be zero-based page 3");
    expect(state.mic.selectedPreset == 5, "mic preset should be zero-based preset 6");
    expect(state.mic.volume == 1.0F, "mic volume should clamp to 1.0");
    expect(state.synth.selectedPreset == 3, "synth preset should be zero-based preset 4");
    expect(state.synth.fxParameters[4] == 0.42F, "synth FX parameter should be stored");
}

void testYaeltexLooperCommands()
{
    LiveLoopingEngine engine;

    engine.handle(yaeltexCommand(CommandType::SelectLooper, 1));
    engine.handle(yaeltexCommand(CommandType::SelectSampleLength, 8));
    engine.handle(yaeltexCommand(CommandType::ToggleTrackRecording, 0));

    auto state = engine.state();
    expect(state.selectedLooper == 1, "selected looper should be zero-based looper 2");
    expect(state.selectedSampleLengthBeats == 8, "selected sample length should be 8 beats");
    expect(state.loopers[1].tracks[0].state == TrackState::Recording, "L2 T1 should start recording");
    expect(state.loopers[1].tracks[0].sampleLengthBeats == 8, "L2 T1 should use selected length");

    engine.handle(yaeltexCommand(CommandType::ToggleTrackRecording, 0));
    state = engine.state();
    expect(state.loopers[1].tracks[0].state == TrackState::Playing, "L2 T1 should switch to playing");

    engine.handle(yaeltexCommand(CommandType::StartResampleAllLoopers));
    engine.handle(yaeltexCommand(CommandType::ToggleTrackRecording, 1));
    state = engine.state();
    expect(state.resampleMode == ResampleMode::AllLoopers, "resample mode should remain all-loopers while resampling");
    expect(state.loopers[1].tracks[1].state == TrackState::Resampling, "L2 T2 should start resampling");

    engine.handle(yaeltexCommand(CommandType::ResetLooper));
    state = engine.state();
    expect(state.loopers[1].tracks[0].state == TrackState::Empty, "reset looper clears L2 T1");
    expect(state.loopers[1].tracks[1].state == TrackState::Empty, "reset looper clears L2 T2");
}

} // namespace

int main()
{
    testInputControllerCommands();
    testYaeltexLooperCommands();

    if (failures != 0) {
        std::cerr << failures << " smoke test assertion(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "core smoke tests passed\n";
    return EXIT_SUCCESS;
}

