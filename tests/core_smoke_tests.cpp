#include "livelooping/core/ControlMapping.h"
#include "livelooping/core/LiveLoopingEngine.h"

#include <cstdlib>
#include <iostream>
#include <string>

using livelooping::core::CommandType;
using livelooping::core::ControllerCommand;
using livelooping::core::ControllerId;
using livelooping::core::InputTarget;
using livelooping::core::LiveLoopingEngine;
using livelooping::core::MidiEvent;
using livelooping::core::MidiMapper;
using livelooping::core::MidiMessageType;
using livelooping::core::ResampleMode;
using livelooping::core::TrackState;
using livelooping::core::WidgetEvent;
using livelooping::core::WidgetEventType;
using livelooping::core::makeMicKaossPadProfile;
using livelooping::core::makeSynthKaossPadProfile;
using livelooping::core::makeYaeltexLiveLoopingProfile;
using livelooping::core::normalizeMidiValue;

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

void testKaossPadMapping()
{
    const MidiMapper micMapper(makeMicKaossPadProfile());
    const auto mappedPreset = micMapper.mapMidi({MidiMessageType::ControlChange, 0, 51, 127});
    expect(mappedPreset.has_value(), "mic preset MIDI event should map to a command");
    expect(mappedPreset->controller == ControllerId::MicKaossPad, "mic preset command should carry mic controller id");
    expect(mappedPreset->inputTarget == InputTarget::Mic, "mic preset command should target mic input");
    expect(mappedPreset->type == CommandType::SelectInputPreset, "mic preset MIDI should select input preset");
    expect(mappedPreset->index == 2, "CC 51 should map to preset 3");

    const auto ignoredRelease = micMapper.mapMidi({MidiMessageType::ControlChange, 0, 51, 0});
    expect(!ignoredRelease.has_value(), "zero-valued button release should not emit a command");

    const auto mappedVolume = micMapper.mapMidi({MidiMessageType::ControlChange, 0, 93, 64});
    expect(mappedVolume.has_value(), "mic volume CC should map to a command");
    expect(mappedVolume->type == CommandType::SetInputVolume, "mic volume CC should set input volume");
    expect(mappedVolume->value > 0.50F && mappedVolume->value < 0.51F, "MIDI 64 should normalize close to 0.5");

    const MidiMapper synthMapper(makeSynthKaossPadProfile());
    const auto mappedWidget = synthMapper.mapWidget({"page_2", WidgetEventType::Press, 1.0F});
    expect(mappedWidget.has_value(), "synth page widget should map to a command");
    expect(mappedWidget->controller == ControllerId::SynthKaossPad, "synth widget command should carry synth controller id");
    expect(mappedWidget->inputTarget == InputTarget::Synth, "synth page widget should target synth input");
    expect(mappedWidget->type == CommandType::SelectInputPresetPage, "synth page widget should select page");
    expect(mappedWidget->index == 1, "page_2 should map to zero-based page 2");
}

void testYaeltexMapping()
{
    const MidiMapper mapper(makeYaeltexLiveLoopingProfile());
    expect(!mapper.profile().widgets.empty(), "Yaeltex profile should expose widgets");
    expect(mapper.profile().widgets.front().group == "looper_select", "Yaeltex widgets should carry layout groups");

    const auto mappedLooper = mapper.mapMidi({MidiMessageType::Note, 0, 61, 127});
    expect(mappedLooper.has_value(), "Yaeltex looper note should map to a command");
    expect(mappedLooper->controller == ControllerId::Yaeltex, "Yaeltex command should carry Yaeltex controller id");
    expect(mappedLooper->type == CommandType::SelectLooper, "Yaeltex looper note should select looper");
    expect(mappedLooper->index == 1, "note 61 should map to looper 2");

    const auto mappedLength = mapper.mapMidi({MidiMessageType::Note, 0, 75, 127});
    expect(mappedLength.has_value(), "Yaeltex sample length note should map to a command");
    expect(mappedLength->type == CommandType::SelectSampleLength, "Yaeltex length note should select sample length");
    expect(mappedLength->index == 8, "note 75 should map to 8 beats");

    const auto mappedRecord = mapper.mapMidi({MidiMessageType::Note, 0, 80, 127});
    expect(mappedRecord.has_value(), "Yaeltex record note should map to a command");
    expect(mappedRecord->type == CommandType::ToggleTrackRecording, "Yaeltex record note should toggle recording");
    expect(mappedRecord->index == 0, "note 80 should map to track 1");

    const auto mappedPseudoKnob = mapper.mapWidget({"vol_pan_t3", WidgetEventType::Change, 0.25F});
    expect(mappedPseudoKnob.has_value(), "Yaeltex pseudo knob should map to a command");
    expect(mappedPseudoKnob->type == CommandType::SetTrackVolume, "Vol/Pan T3 should set track volume initially");
    expect(mappedPseudoKnob->index == 2, "Vol/Pan T3 should target zero-based track 3");
    expect(mappedPseudoKnob->value == 0.25F, "pseudo knob value should pass through");

    expect(normalizeMidiValue(-10) == 0.0F, "negative MIDI values should clamp to zero");
    expect(normalizeMidiValue(200) == 1.0F, "large MIDI values should clamp to one");
}

} // namespace

int main()
{
    testInputControllerCommands();
    testYaeltexLooperCommands();
    testKaossPadMapping();
    testYaeltexMapping();

    if (failures != 0) {
        std::cerr << failures << " smoke test assertion(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "core smoke tests passed\n";
    return EXIT_SUCCESS;
}
