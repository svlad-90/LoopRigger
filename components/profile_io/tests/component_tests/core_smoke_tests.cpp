#include "loop_rigger/control/ControlMapping.h"
#include "loop_rigger/core/LiveLoopingEngine.h"

#if LIVELOOPING_HAS_PROFILE_IO
#include "loop_rigger/profile_io/ProfileLoader.h"
#include "loop_rigger/profile_io/SurfaceLayoutGeometry.h"
#endif

#include <cstdlib>
#include <set>
#include <iostream>
#include <string>

using loop_rigger::core::CommandType;
using loop_rigger::core::ControllerCommand;
using loop_rigger::core::ControllerId;
using loop_rigger::core::InputTarget;
using loop_rigger::core::LiveLoopingEngine;
using loop_rigger::control::MidiEvent;
using loop_rigger::control::MidiMapper;
using loop_rigger::control::MidiMessageType;
using loop_rigger::core::ResampleMode;
using loop_rigger::core::TrackState;
using loop_rigger::control::WidgetEvent;
using loop_rigger::control::WidgetEventType;
using loop_rigger::control::makeMicKaossPadProfile;
using loop_rigger::control::makeSynthKaossPadProfile;
using loop_rigger::control::makeYaeltexLiveLoopingProfile;
using loop_rigger::control::normalizeMidiValue;
#if LIVELOOPING_HAS_PROFILE_IO
using loop_rigger::profile_io::loadControllerProfileFromFile;
using loop_rigger::profile_io::loadControlSurfaceLayoutFromFile;
using loop_rigger::profile_io::loadDevicePackageFromDirectory;
using loop_rigger::profile_io::hasScript;
using loop_rigger::profile_io::containsSurfaceBounds;
using loop_rigger::profile_io::findSurfaceGroupGeometry;
using loop_rigger::profile_io::profileSurfaceLayout;
using loop_rigger::profile_io::summarizeSurfaceGroups;
using loop_rigger::profile_io::surfaceBoundsOverlap;
#endif

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
    engine.handle(inputCommand(InputTarget::Mic, CommandType::ToggleInputFxHold));
    engine.handle(inputCommand(InputTarget::Synth, CommandType::SelectInputPreset, 3));
    engine.handle(inputCommand(InputTarget::Synth, CommandType::SetInputFxParameter, 4, 0.42F));

    const auto& state = engine.state();
    expect(state.mic.selectedPage == 2, "mic page should be zero-based page 3");
    expect(state.mic.selectedPreset == 5, "mic preset should be zero-based preset 6");
    expect(state.mic.volume == 1.0F, "mic volume should clamp to 1.0");
    expect(state.mic.fxHold, "mic FX hold should toggle on");
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

void testYaeltexPerformanceCommands()
{
    LiveLoopingEngine engine;

    engine.handle(yaeltexCommand(CommandType::StartTransport));
    engine.handle(yaeltexCommand(CommandType::RestartAllLoopers));
    engine.handle(yaeltexCommand(CommandType::SelectRoutingSource, 0));
    engine.handle(yaeltexCommand(CommandType::SelectRoutingTarget, 2));
    engine.handle(yaeltexCommand(CommandType::SelectCenterFxSlot, 4));
    engine.handle(yaeltexCommand(CommandType::SelectCenterFxBank, 2));
    engine.handle(yaeltexCommand(CommandType::SetCenterFxParameter, 0, 0.75F));
    engine.handle(yaeltexCommand(CommandType::SetCenterFxJoystick, 1, 0.25F));
    engine.handle(yaeltexCommand(CommandType::SetMasterParameter, 4, 0.5F));
    engine.handle(yaeltexCommand(CommandType::TriggerSamplerSlot, 6));
    engine.handle(yaeltexCommand(CommandType::SetLooperVolume, 3, 0.35F));
    engine.handle(yaeltexCommand(CommandType::ToggleTrackSelection, 2));
    engine.handle(yaeltexCommand(CommandType::ToggleTrackMute, 1));
    engine.handle(yaeltexCommand(CommandType::ToggleLooperMute, 2));
    engine.handle(yaeltexCommand(CommandType::ToggleTrackInvert, 3));
    engine.handle(yaeltexCommand(CommandType::ToggleLooperInvert, 1));
    engine.handle(yaeltexCommand(CommandType::SelectClockDivision, 5));
    engine.handle(yaeltexCommand(CommandType::SetTopFxParameter, 1, 0.45F));
    engine.handle(yaeltexCommand(CommandType::TriggerRemixerMode, 18));
    engine.handle(yaeltexCommand(CommandType::TriggerAnimationSlot, 3));
    engine.handle(yaeltexCommand(CommandType::TriggerPresetSlot, 6));
    engine.handle(yaeltexCommand(CommandType::SetSidechainParameter, 7, 0.25F));

    const auto& state = engine.state();
    expect(state.transport.playing, "transport should be playing");
    expect(state.transport.restartGeneration == 1, "restart all should increment restart generation");
    expect(state.routing.source == loop_rigger::core::RoutingSource::Mic, "routing source should select mic");
    expect(state.routing.target == loop_rigger::core::RoutingTarget::Sampler, "routing target should select sampler");
    expect(state.centerFx.selectedSlot == 4, "center FX should select zero-based slot 5");
    expect(state.centerFx.selectedBank == 2, "center FX should select zero-based bank 3");
    expect(state.centerFx.parameters[0] == 0.75F, "center FX parameter should store value");
    expect(state.centerFx.joysticks[1] == 0.25F, "center FX joystick should store value");
    expect(state.master.parameters[4] == 0.5F, "master parameter should store value");
    expect(state.sampler.slots[6].loaded, "sampler slot should become loaded");
    expect(state.sampler.slots[6].playing, "sampler slot should become playing");
    expect(state.loopers[3].volume == 0.35F, "looper volume command should target its indexed looper");
    expect(state.loopers[state.selectedLooper].tracks[2].selected, "track selection command should toggle track selection on the selected looper");
    expect(state.loopers[state.selectedLooper].tracks[1].muted, "track mute command should toggle selected looper track mute");
    expect(state.loopers[2].muted, "looper mute command should toggle indexed looper mute");
    expect(state.loopers[state.selectedLooper].tracks[3].inverted, "track invert command should toggle selected looper track invert");
    expect(state.loopers[1].inverted, "looper invert command should toggle indexed looper invert");
    expect(state.yaeltex.selectedClockDivision == 5, "clock division command should store selected division");
    expect(state.yaeltex.topFxParameters[1] == 0.45F, "top FX parameter should store value");
    expect(state.yaeltex.selectedRemixerMode == 18, "remixer mode command should store selected mode");
    expect(state.yaeltex.selectedAnimationSlot == 3, "animation command should store selected slot");
    expect(state.yaeltex.selectedPresetSlot == 6, "preset command should store selected slot");
    expect(state.yaeltex.sidechainParameters[7] == 0.25F, "sidechain parameter should store value");
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

    const auto mappedVolumeKnob = micMapper.mapWidget({"input_volume_knob", WidgetEventType::Change, 0.25F});
    expect(mappedVolumeKnob.has_value(), "mic input volume knob widget should map to a command");
    expect(mappedVolumeKnob->type == CommandType::SetInputVolume, "mic input volume knob should set input volume");
    expect(mappedVolumeKnob->value == 0.25F, "mic input volume knob should use event value");

    const auto mappedVolumeFader = micMapper.mapWidget({"input_volume_fader", WidgetEventType::Change, 0.75F});
    expect(mappedVolumeFader.has_value(), "mic input volume fader widget should map to a command");
    expect(mappedVolumeFader->type == CommandType::SetInputVolume, "mic input volume fader should set input volume");
    expect(mappedVolumeFader->value == 0.75F, "mic input volume fader should use event value");

    const auto mappedFxKnob = micMapper.mapWidget({"fx_level_knob", WidgetEventType::Change, 0.35F});
    expect(mappedFxKnob.has_value(), "mic FX level knob widget should map to a command");
    expect(mappedFxKnob->type == CommandType::SetInputFxLevel, "mic FX level knob should set input FX level");
    expect(mappedFxKnob->value == 0.35F, "mic FX level knob should use event value");

    const auto mappedFxFader = micMapper.mapWidget({"fx_level_fader", WidgetEventType::Change, 0.65F});
    expect(mappedFxFader.has_value(), "mic FX level fader widget should map to a command");
    expect(mappedFxFader->type == CommandType::SetInputFxLevel, "mic FX level fader should set input FX level");
    expect(mappedFxFader->value == 0.65F, "mic FX level fader should use event value");

    const auto mappedHold = micMapper.mapWidget({"hold", WidgetEventType::Press, 1.0F});
    expect(mappedHold.has_value(), "mic hold widget should map to a command");
    expect(mappedHold->type == CommandType::ToggleInputFxHold, "mic hold widget should toggle FX hold");
    expect(mappedHold->inputTarget == InputTarget::Mic, "mic hold widget should target mic input");

    const MidiMapper synthMapper(makeSynthKaossPadProfile());
    const auto mappedWidget = synthMapper.mapWidget({"page_2", WidgetEventType::Press, 1.0F});
    expect(mappedWidget.has_value(), "synth page widget should map to a command");
    expect(mappedWidget->controller == ControllerId::SynthKaossPad, "synth widget command should carry synth controller id");
    expect(mappedWidget->inputTarget == InputTarget::Synth, "synth page widget should target synth input");
    expect(mappedWidget->type == CommandType::SelectInputPresetPage, "synth page widget should select page");
    expect(mappedWidget->index == 1, "page_2 should map to zero-based page 2");

    const auto mappedSynthHold = synthMapper.mapWidget({"hold", WidgetEventType::Press, 1.0F});
    expect(mappedSynthHold.has_value(), "synth hold widget should map to a command");
    expect(mappedSynthHold->type == CommandType::ToggleInputFxHold, "synth hold widget should toggle FX hold");
    expect(mappedSynthHold->inputTarget == InputTarget::Synth, "synth hold widget should target synth input");
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

    const auto mappedRoutingSource = mapper.mapWidget({"routing_mic", WidgetEventType::Press, 1.0F});
    expect(mappedRoutingSource.has_value(), "Yaeltex routing widget should map to a command");
    expect(mappedRoutingSource->type == CommandType::SelectRoutingSource, "MIC should select routing source");
    expect(mappedRoutingSource->index == 0, "MIC should map to routing source index 0");

    const auto mappedFxSlot = mapper.mapWidget({"center_fx_slot_5", WidgetEventType::Press, 1.0F});
    expect(mappedFxSlot.has_value(), "Yaeltex FX slot widget should map to a command");
    expect(mappedFxSlot->type == CommandType::SelectCenterFxSlot, "FX slot should select center FX slot");
    expect(mappedFxSlot->index == 4, "FX5 should map to zero-based slot 5");

    const auto mappedSamplerSlot = mapper.mapWidget({"sampler_slot_7", WidgetEventType::Press, 1.0F});
    expect(mappedSamplerSlot.has_value(), "Yaeltex sampler widget should map to a command");
    expect(mappedSamplerSlot->type == CommandType::TriggerSamplerSlot, "sampler button should trigger sampler slot");
    expect(mappedSamplerSlot->index == 6, "sampler button 7 should map to zero-based slot 7");

    const auto mappedTrackSelect = mapper.mapWidget({"select_t3", WidgetEventType::Press, 1.0F});
    expect(mappedTrackSelect.has_value(), "Yaeltex track select widget should map to a command");
    expect(mappedTrackSelect->type == CommandType::ToggleTrackSelection, "Select T3 should toggle track selection");
    expect(mappedTrackSelect->index == 2, "Select T3 should map to zero-based track 3");

    const auto mappedLooperVolume = mapper.mapWidget({"vol_pan_l4", WidgetEventType::Change, 0.4F});
    expect(mappedLooperVolume.has_value(), "Yaeltex looper volume widget should map to a command");
    expect(mappedLooperVolume->type == CommandType::SetLooperVolume, "Vol/Pan L4 should set looper volume");
    expect(mappedLooperVolume->index == 3, "Vol/Pan L4 should map to zero-based looper 4");
    expect(mappedLooperVolume->value == 0.4F, "looper volume value should pass through");

    const auto mappedClockDivision = mapper.mapWidget({"top_grid_6", WidgetEventType::Press, 1.0F});
    expect(mappedClockDivision.has_value(), "Yaeltex clock division widget should map to a command");
    expect(mappedClockDivision->type == CommandType::SelectClockDivision, "top grid should select clock division");
    expect(mappedClockDivision->index == 5, "top_grid_6 should map to zero-based clock division 6");

    const auto mappedMuteTrack = mapper.mapWidget({"mute_2", WidgetEventType::Press, 1.0F});
    expect(mappedMuteTrack.has_value(), "Yaeltex Mute T2 widget should map to a command");
    expect(mappedMuteTrack->type == CommandType::ToggleTrackMute, "Mute T2 should toggle track mute");
    expect(mappedMuteTrack->index == 1, "Mute T2 should map to zero-based track 2");

    const auto mappedMuteLooper = mapper.mapWidget({"mute_8", WidgetEventType::Press, 1.0F});
    expect(mappedMuteLooper.has_value(), "Yaeltex Mute L4 widget should map to a command");
    expect(mappedMuteLooper->type == CommandType::ToggleLooperMute, "Mute L4 should toggle looper mute");
    expect(mappedMuteLooper->index == 3, "Mute L4 should map to zero-based looper 4");

    const auto mappedInvertTrack = mapper.mapWidget({"mute_11", WidgetEventType::Press, 1.0F});
    expect(mappedInvertTrack.has_value(), "Yaeltex Inv T3 widget should map to a command");
    expect(mappedInvertTrack->type == CommandType::ToggleTrackInvert, "Inv T3 should toggle track invert");
    expect(mappedInvertTrack->index == 2, "Inv T3 should map to zero-based track 3");

    const auto mappedInvertLooper = mapper.mapWidget({"mute_14", WidgetEventType::Press, 1.0F});
    expect(mappedInvertLooper.has_value(), "Yaeltex Inv L2 widget should map to a command");
    expect(mappedInvertLooper->type == CommandType::ToggleLooperInvert, "Inv L2 should toggle looper invert");
    expect(mappedInvertLooper->index == 1, "Inv L2 should map to zero-based looper 2");

    const auto mappedTopFx = mapper.mapWidget({"top_reverb", WidgetEventType::Change, 0.7F});
    expect(mappedTopFx.has_value(), "Yaeltex top FX knob should map to a command");
    expect(mappedTopFx->type == CommandType::SetTopFxParameter, "top Reverb should set top FX parameter");
    expect(mappedTopFx->index == 1, "top Reverb should map to zero-based top FX parameter 2");
    expect(mappedTopFx->value == 0.7F, "top FX value should pass through");

    const auto mappedRemixerMode = mapper.mapWidget({"remixer_mode_19", WidgetEventType::Press, 1.0F});
    expect(mappedRemixerMode.has_value(), "Yaeltex remixer mode widget should map to a command");
    expect(mappedRemixerMode->type == CommandType::TriggerRemixerMode, "remixer mode button should trigger remixer mode");
    expect(mappedRemixerMode->index == 18, "remixer_mode_19 should map to zero-based mode 19");

    const auto mappedAnimation = mapper.mapWidget({"animation_4", WidgetEventType::Press, 1.0F});
    expect(mappedAnimation.has_value(), "Yaeltex animation widget should map to a command");
    expect(mappedAnimation->type == CommandType::TriggerAnimationSlot, "animation button should trigger animation slot");
    expect(mappedAnimation->index == 3, "animation_4 should map to zero-based animation slot 4");

    const auto mappedPresetSlot = mapper.mapWidget({"preset_pr5", WidgetEventType::Press, 1.0F});
    expect(mappedPresetSlot.has_value(), "Yaeltex preset slot widget should map to a command");
    expect(mappedPresetSlot->type == CommandType::TriggerPresetSlot, "preset button should trigger preset slot");
    expect(mappedPresetSlot->index == 6, "preset_pr5 should map to zero-based preset slot 7");

    const auto mappedSidechain = mapper.mapWidget({"sidechain_decay_t4", WidgetEventType::Change, 0.3F});
    expect(mappedSidechain.has_value(), "Yaeltex sidechain knob should map to a command");
    expect(mappedSidechain->type == CommandType::SetSidechainParameter, "sidechain knob should set sidechain parameter");
    expect(mappedSidechain->index == 7, "sidechain_decay_t4 should map to zero-based sidechain parameter 8");
    expect(mappedSidechain->value == 0.3F, "sidechain value should pass through");

    const auto mappedBottomRecord = mapper.mapWidget({"bottom_record", WidgetEventType::Press, 1.0F});
    expect(mappedBottomRecord.has_value(), "Yaeltex bottom record widget should map to a command");
    expect(mappedBottomRecord->type == CommandType::ToggleTrackRecording, "bottom Record should toggle selected track recording");

    const auto mappedBottomClear = mapper.mapWidget({"bottom_extra_clear_l", WidgetEventType::Press, 1.0F});
    expect(mappedBottomClear.has_value(), "Yaeltex bottom Extra/Clear L widget should map to a command");
    expect(mappedBottomClear->type == CommandType::ResetLooper, "bottom Extra/Clear L should reset selected looper");

    expect(normalizeMidiValue(-10) == 0.0F, "negative MIDI values should clamp to zero");
    expect(normalizeMidiValue(200) == 1.0F, "large MIDI values should clamp to one");
}

#if LIVELOOPING_HAS_PROFILE_IO
std::string profilePath(const std::string& fileName)
{
    return std::string(LIVELOOPING_PROFILE_DIR) + "/" + fileName;
}

std::string layoutPath(const std::string& fileName)
{
    return std::string(LIVELOOPING_LAYOUT_DIR) + "/" + fileName;
}

std::string devicePath(const std::string& directoryName)
{
    return std::string(LIVELOOPING_DEVICE_DIR) + "/" + directoryName;
}

std::set<std::string> widgetIds(const loop_rigger::control::ControllerProfile& profile)
{
    std::set<std::string> ids;
    for (const auto& widget : profile.widgets) {
        ids.insert(widget.id);
    }
    return ids;
}

void expectLayoutWidgetsBelongToProfile(
    const loop_rigger::profile_io::ControlSurfaceLayout& layout,
    const loop_rigger::control::ControllerProfile& profile)
{
    const auto ids = widgetIds(profile);
    for (const auto& element : layout.elements) {
        if (element.role != loop_rigger::profile_io::SurfaceElementRole::Widget) {
            continue;
        }
        expect(!element.widgetId.empty(), "layout widget elements should name a profile widget");
        expect(ids.count(element.widgetId) == 1, "layout widget should exist in controller profile: " + element.widgetId);
        expect(element.bounds.width > 0.0F, "layout widget should have positive width: " + element.id);
        expect(element.bounds.height > 0.0F, "layout widget should have positive height: " + element.id);
        expect(element.bounds.x >= 0.0F, "layout widget should stay inside canvas horizontally: " + element.id);
        expect(element.bounds.y >= 0.0F, "layout widget should stay inside canvas vertically: " + element.id);
        expect(element.bounds.x + element.bounds.width <= static_cast<float>(layout.baseWidth), "layout widget should not exceed canvas width: " + element.id);
        expect(element.bounds.y + element.bounds.height <= static_cast<float>(layout.baseHeight), "layout widget should not exceed canvas height: " + element.id);
    }
}

void expectLayoutElementsStayInsideCanvas(const loop_rigger::profile_io::ControlSurfaceLayout& layout)
{
    for (const auto& element : layout.elements) {
        expect(element.bounds.width >= 0.0F, "layout element should have non-negative width: " + element.id);
        expect(element.bounds.height >= 0.0F, "layout element should have non-negative height: " + element.id);
        expect(element.bounds.x >= 0.0F, "layout element should stay inside canvas horizontally: " + element.id);
        expect(element.bounds.y >= 0.0F, "layout element should stay inside canvas vertically: " + element.id);
        expect(element.bounds.x + element.bounds.width <= static_cast<float>(layout.baseWidth), "layout element should not exceed canvas width: " + element.id);
        expect(element.bounds.y + element.bounds.height <= static_cast<float>(layout.baseHeight), "layout element should not exceed canvas height: " + element.id);
    }
}

void expectBoundsEqual(
    const loop_rigger::profile_io::SurfaceBounds& bounds,
    float x,
    float y,
    float width,
    float height,
    const std::string& message)
{
    expect(bounds.x == x, message + " x");
    expect(bounds.y == y, message + " y");
    expect(bounds.width == width, message + " width");
    expect(bounds.height == height, message + " height");
}

void expectGroupGeometry(
    const loop_rigger::profile_io::ControlSurfaceLayout& layout,
    const std::string& group,
    size_t widgetCount,
    float x,
    float y,
    float width,
    float height)
{
    const auto geometry = findSurfaceGroupGeometry(layout, group);
    expect(geometry.has_value(), "layout should expose group geometry: " + group);
    if (!geometry.has_value()) {
        return;
    }

    expect(geometry->elementCount == widgetCount, "group element count should match widget count: " + group);
    expect(geometry->widgetCount == widgetCount, "group widget count should match: " + group);
    expect(geometry->decorationCount == 0, "group should contain only widgets: " + group);
    expectBoundsEqual(geometry->bounds, x, y, width, height, "group bounds should match: " + group);
}

void expectLayoutWidgetGroupsBelongToProfile(
    const loop_rigger::profile_io::ControlSurfaceLayout& layout,
    const loop_rigger::control::ControllerProfile& profile)
{
    const auto profileGroups = [&profile] {
        std::set<std::string> groups;
        for (const auto& widget : profile.widgets) {
            if (!widget.group.empty()) {
                groups.insert(widget.group);
            }
        }
        return groups;
    }();

    for (const auto& geometry : summarizeSurfaceGroups(layout)) {
        expect(profileGroups.count(geometry.group) == 1, "layout widget group should exist in controller profile: " + geometry.group);
    }
}

void expectGroupBoundsContainTheirElements(const loop_rigger::profile_io::ControlSurfaceLayout& layout)
{
    for (const auto& element : layout.elements) {
        if (element.group.empty()) {
            continue;
        }

        const auto geometry = findSurfaceGroupGeometry(layout, element.group);
        expect(geometry.has_value(), "group geometry should exist for element: " + element.id);
        if (geometry.has_value()) {
            expect(containsSurfaceBounds(geometry->bounds, element.bounds), "group bounds should contain element: " + element.id);
        }
    }
}

void expectGroupedWidgetsDoNotOverlap(const loop_rigger::profile_io::ControlSurfaceLayout& layout)
{
    for (size_t lhs = 0; lhs < layout.elements.size(); ++lhs) {
        const auto& left = layout.elements[lhs];
        if (left.group.empty() || left.role != loop_rigger::profile_io::SurfaceElementRole::Widget) {
            continue;
        }

        for (size_t rhs = lhs + 1; rhs < layout.elements.size(); ++rhs) {
            const auto& right = layout.elements[rhs];
            if (right.group == left.group && right.role == loop_rigger::profile_io::SurfaceElementRole::Widget) {
                expect(!surfaceBoundsOverlap(left.bounds, right.bounds), "grouped widgets should not overlap: " + left.id + " / " + right.id);
            }
        }
    }
}

void expectWidgetGroupsDoNotOverlap(const loop_rigger::profile_io::ControlSurfaceLayout& layout)
{
    const auto groups = summarizeSurfaceGroups(layout);
    for (size_t lhs = 0; lhs < groups.size(); ++lhs) {
        for (size_t rhs = lhs + 1; rhs < groups.size(); ++rhs) {
            expect(
                !surfaceBoundsOverlap(groups[lhs].bounds, groups[rhs].bounds),
                "widget groups should not overlap: " + groups[lhs].group + " / " + groups[rhs].group);
        }
    }
}

void testJsonProfileLoading()
{
    const MidiMapper micMapper(loadControllerProfileFromFile(profilePath("kaoss_mic.json")));
    const auto mappedPreset = micMapper.mapMidi({MidiMessageType::ControlChange, 0, 51, 127});
    expect(mappedPreset.has_value(), "JSON mic profile should map preset MIDI");
    expect(mappedPreset->controller == ControllerId::MicKaossPad, "JSON mic command should carry mic controller id");
    expect(mappedPreset->inputTarget == InputTarget::Mic, "JSON mic command should target mic input");
    expect(mappedPreset->type == CommandType::SelectInputPreset, "JSON mic preset should select input preset");
    expect(mappedPreset->index == 2, "JSON mic CC 51 should map to preset 3");

    const MidiMapper synthMapper(loadControllerProfileFromFile(profilePath("kaoss_synth.json")));
    const auto mappedPage = synthMapper.mapWidget({"page_2", WidgetEventType::Press, 1.0F});
    expect(mappedPage.has_value(), "JSON synth profile should map page widget");
    expect(mappedPage->controller == ControllerId::SynthKaossPad, "JSON synth command should carry synth controller id");
    expect(mappedPage->inputTarget == InputTarget::Synth, "JSON synth page should target synth input");
    expect(mappedPage->index == 1, "JSON synth page_2 should map to zero-based page 2");

    const auto mappedSynthFxFader = synthMapper.mapWidget({"fx_level_fader", WidgetEventType::Change, 0.5F});
    expect(mappedSynthFxFader.has_value(), "JSON synth profile should map FX level fader widget");
    expect(mappedSynthFxFader->controller == ControllerId::SynthKaossPad, "JSON synth fader command should carry synth controller id");
    expect(mappedSynthFxFader->inputTarget == InputTarget::Synth, "JSON synth fader command should target synth input");
    expect(mappedSynthFxFader->type == CommandType::SetInputFxLevel, "JSON synth fader should set input FX level");
    expect(mappedSynthFxFader->value == 0.5F, "JSON synth fader should use event value");

    const MidiMapper yaeltexMapper(loadControllerProfileFromFile(profilePath("yaeltex_livelooping.json")));
    const auto mappedLength = yaeltexMapper.mapMidi({MidiMessageType::Note, 0, 75, 127});
    expect(mappedLength.has_value(), "JSON Yaeltex profile should map sample length note");
    expect(mappedLength->controller == ControllerId::Yaeltex, "JSON Yaeltex command should carry Yaeltex controller id");
    expect(mappedLength->type == CommandType::SelectSampleLength, "JSON Yaeltex length should select sample length");
    expect(mappedLength->index == 8, "JSON Yaeltex note 75 should map to 8 beats");

    const auto mappedKnob = yaeltexMapper.mapWidget({"vol_pan_t3", WidgetEventType::Change, 0.42F});
    expect(mappedKnob.has_value(), "JSON Yaeltex profile should map pseudo knob");
    expect(mappedKnob->type == CommandType::SetTrackVolume, "JSON Vol/Pan T3 should set track volume");
    expect(mappedKnob->index == 2, "JSON Vol/Pan T3 should target zero-based track 3");
    expect(mappedKnob->value == 0.42F, "JSON pseudo knob value should pass through");

    const auto mappedFxBank = yaeltexMapper.mapWidget({"center_fx_bank_3", WidgetEventType::Press, 1.0F});
    expect(mappedFxBank.has_value(), "JSON Yaeltex profile should map center FX bank");
    expect(mappedFxBank->type == CommandType::SelectCenterFxBank, "JSON B3 should select center FX bank");
    expect(mappedFxBank->index == 2, "JSON B3 should map to zero-based bank 3");

    const auto mappedMaster = yaeltexMapper.mapWidget({"master_output_volume", WidgetEventType::Change, 0.66F});
    expect(mappedMaster.has_value(), "JSON Yaeltex profile should map master output volume");
    expect(mappedMaster->type == CommandType::SetMasterParameter, "JSON master output should set master parameter");
    expect(mappedMaster->index == 4, "JSON output volume should map to master parameter 5");
    expect(mappedMaster->value == 0.66F, "JSON master value should pass through");

    const auto mappedTrackSelect = yaeltexMapper.mapWidget({"select_t2", WidgetEventType::Press, 1.0F});
    expect(mappedTrackSelect.has_value(), "JSON Yaeltex profile should map track select");
    expect(mappedTrackSelect->type == CommandType::ToggleTrackSelection, "JSON Select T2 should toggle track selection");
    expect(mappedTrackSelect->index == 1, "JSON Select T2 should target zero-based track 2");

    const auto mappedLooperVolume = yaeltexMapper.mapWidget({"vol_pan_l1", WidgetEventType::Change, 0.2F});
    expect(mappedLooperVolume.has_value(), "JSON Yaeltex profile should map looper volume");
    expect(mappedLooperVolume->type == CommandType::SetLooperVolume, "JSON Vol/Pan L1 should set looper volume");
    expect(mappedLooperVolume->index == 0, "JSON Vol/Pan L1 should target zero-based looper 1");
    expect(mappedLooperVolume->value == 0.2F, "JSON looper volume should pass through widget value");

    const auto mappedClockDivision = yaeltexMapper.mapWidget({"top_grid_8", WidgetEventType::Press, 1.0F});
    expect(mappedClockDivision.has_value(), "JSON Yaeltex profile should map clock division");
    expect(mappedClockDivision->type == CommandType::SelectClockDivision, "JSON top_grid_8 should select clock division");
    expect(mappedClockDivision->index == 7, "JSON top_grid_8 should map to zero-based clock division 8");

    const auto mappedSidechain = yaeltexMapper.mapWidget({"sidechain_stash_t2", WidgetEventType::Change, 0.55F});
    expect(mappedSidechain.has_value(), "JSON Yaeltex profile should map sidechain knob");
    expect(mappedSidechain->type == CommandType::SetSidechainParameter, "JSON sidechain knob should set sidechain parameter");
    expect(mappedSidechain->index == 1, "JSON sidechain_stash_t2 should map to zero-based sidechain parameter 2");
    expect(mappedSidechain->value == 0.55F, "JSON sidechain value should pass through");

    const auto mappedMute = yaeltexMapper.mapWidget({"mute_15", WidgetEventType::Press, 1.0F});
    expect(mappedMute.has_value(), "JSON Yaeltex profile should map Inv L3");
    expect(mappedMute->type == CommandType::ToggleLooperInvert, "JSON Inv L3 should toggle looper invert");
    expect(mappedMute->index == 2, "JSON Inv L3 should target zero-based looper 3");
}

void testJsonSurfaceLayoutLoading()
{
    const auto kaossLayout = loadControlSurfaceLayoutFromFile(layoutPath("kaoss_pad.json"));
    expect(kaossLayout.id == "kaoss_pad_kp3", "Kaoss layout should load id");
    expect(kaossLayout.baseWidth == 1200, "Kaoss layout should expose base width");
    expect(kaossLayout.baseHeight == 820, "Kaoss layout should expose base height");
    expect(kaossLayout.elements.size() >= 35, "Kaoss layout should include hardware and widget elements");
    expect(!kaossLayout.elements.front().variant.empty(), "layout elements should load render variants");

    const auto micProfile = loadControllerProfileFromFile(profilePath("kaoss_mic.json"));
    expectLayoutElementsStayInsideCanvas(kaossLayout);
    expectLayoutWidgetsBelongToProfile(kaossLayout, micProfile);
    expect(summarizeSurfaceGroups(kaossLayout).size() == 4, "Kaoss layout should expose four widget groups");
    expectLayoutWidgetGroupsBelongToProfile(kaossLayout, micProfile);
    expectGroupBoundsContainTheirElements(kaossLayout);
    expectGroupedWidgetsDoNotOverlap(kaossLayout);
    expectGroupGeometry(kaossLayout, "hold", 1, 64.0F, 662.0F, 94.0F, 46.0F);
    expectGroupGeometry(kaossLayout, "levels", 4, 76.0F, 100.0F, 84.0F, 478.0F);
    expectGroupGeometry(kaossLayout, "pages", 4, 342.0F, 712.0F, 542.0F, 46.0F);
    expectGroupGeometry(kaossLayout, "presets", 8, 322.0F, 247.0F, 628.0F, 44.0F);

    const auto yaeltexLayout = loadControlSurfaceLayoutFromFile(layoutPath("yaeltex_livelooping.json"));
    expect(yaeltexLayout.id == "yaeltex_livelooping", "Yaeltex layout should load id");
    expect(yaeltexLayout.profileId == "yaeltex.livelooping", "Yaeltex layout should target Yaeltex profile");
    expect(yaeltexLayout.baseWidth == 1520, "Yaeltex layout should expose base width");
    expect(yaeltexLayout.baseHeight == 1080, "Yaeltex layout should expose base height");
    expect(yaeltexLayout.elements.size() >= 80, "Yaeltex layout should include dense faceplate elements");

    const auto yaeltexProfile = loadControllerProfileFromFile(profilePath("yaeltex_livelooping.json"));
    expectLayoutElementsStayInsideCanvas(yaeltexLayout);
    expectLayoutWidgetsBelongToProfile(yaeltexLayout, yaeltexProfile);
    expect(summarizeSurfaceGroups(yaeltexLayout).size() == 26, "Yaeltex layout should expose twenty-six widget groups");
    expectLayoutWidgetGroupsBelongToProfile(yaeltexLayout, yaeltexProfile);
    expectGroupBoundsContainTheirElements(yaeltexLayout);
    expectGroupedWidgetsDoNotOverlap(yaeltexLayout);
    expectWidgetGroupsDoNotOverlap(yaeltexLayout);
    expectGroupGeometry(yaeltexLayout, "center_fx_bank", 5, 620.0F, 540.0F, 326.0F, 30.0F);
    expectGroupGeometry(yaeltexLayout, "center_fx_joystick", 2, 638.0F, 612.0F, 310.0F, 110.0F);
    expectGroupGeometry(yaeltexLayout, "center_fx_parameter", 4, 632.0F, 326.0F, 400.0F, 86.0F);
    expectGroupGeometry(yaeltexLayout, "clock_division", 8, 760.0F, 86.0F, 270.0F, 80.0F);
    expectGroupGeometry(yaeltexLayout, "looper_select", 4, 112.0F, 360.0F, 376.0F, 64.0F);
    expectGroupGeometry(yaeltexLayout, "master_parameter", 8, 1300.0F, 318.0F, 168.0F, 414.0F);
    expectGroupGeometry(yaeltexLayout, "mute_invert", 16, 158.0F, 220.0F, 558.0F, 82.0F);
    expectGroupGeometry(yaeltexLayout, "remixer_mode", 20, 1062.0F, 466.0F, 228.0F, 222.0F);
    expectGroupGeometry(yaeltexLayout, "sample_length", 8, 112.0F, 462.0F, 256.0F, 94.0F);
    expectGroupGeometry(yaeltexLayout, "sampler", 8, 1085.0F, 745.0F, 286.0F, 120.0F);
    expectGroupGeometry(yaeltexLayout, "sidechain_parameter", 8, 620.0F, 846.0F, 384.0F, 182.0F);
    expectGroupGeometry(yaeltexLayout, "top_fx_parameter", 4, 1058.0F, 86.0F, 400.0F, 86.0F);
    expectGroupGeometry(yaeltexLayout, "track_volume_pan", 4, 100.0F, 744.0F, 490.0F, 92.0F);
    expectGroupGeometry(yaeltexLayout, "track_select", 4, 112.0F, 700.0F, 376.0F, 28.0F);
    expectGroupGeometry(yaeltexLayout, "looper_volume_pan", 4, 92.0F, 946.0F, 466.0F, 84.0F);
    expectGroupGeometry(yaeltexLayout, "animation_slot", 8, 620.0F, 752.0F, 408.0F, 28.0F);
    expectGroupGeometry(yaeltexLayout, "preset_slot", 8, 620.0F, 798.0F, 408.0F, 28.0F);
    expectGroupGeometry(yaeltexLayout, "bottom_record", 1, 112.0F, 888.0F, 76.0F, 34.0F);
    expectGroupGeometry(yaeltexLayout, "bottom_extra_clear", 1, 516.0F, 888.0F, 92.0F, 34.0F);
}

void testSurfaceLayoutInteractionProfile()
{
    const auto yaeltexLayout = loadControlSurfaceLayoutFromFile(layoutPath("yaeltex_livelooping.json"));
    const auto profile = profileSurfaceLayout(yaeltexLayout);

    expect(profile.elementCount == yaeltexLayout.elements.size(), "layout profile should count all elements");
    expect(profile.widgetCount > 100, "Yaeltex layout should expose a large interactive widget set");
    expect(profile.decorationCount > 0, "Yaeltex layout should have a cacheable static decoration layer");
    expect(profile.widgetCount < profile.elementCount, "Yaeltex interaction hit-testing should be able to skip decorations");
}

void testDevicePackageLoading()
{
    const auto micPackage = loadDevicePackageFromDirectory(devicePath("kaoss_mic"));
    expect(micPackage.manifest.id == "kaoss.mic", "mic package should load id");
    expect(micPackage.controllerProfile.id == "kaoss.mic", "mic package should load matching controller profile");
    expect(micPackage.controlSurfaceLayout.id == "kaoss_pad_kp3", "mic package should load shared Kaoss layout");
    expect(hasScript(micPackage.manifest), "mic package should expose placeholder script path");

    const auto synthPackage = loadDevicePackageFromDirectory(devicePath("kaoss_synth"));
    expect(synthPackage.manifest.id == "kaoss.synth", "synth package should load id");
    expect(synthPackage.controllerProfile.controller == ControllerId::SynthKaossPad, "synth package should load synth profile");
    expect(synthPackage.controlSurfaceLayout.profileId == "kaoss.*", "synth package should use shared Kaoss layout profile id");

    const auto yaeltexPackage = loadDevicePackageFromDirectory(devicePath("yaeltex_livelooping"));
    expect(yaeltexPackage.manifest.id == "yaeltex.livelooping", "Yaeltex package should load id");
    expect(yaeltexPackage.controllerProfile.id == "yaeltex.livelooping", "Yaeltex package should load matching profile");
    expect(yaeltexPackage.controlSurfaceLayout.profileId == "yaeltex.livelooping", "Yaeltex package should load matching layout");
    expect(!yaeltexPackage.manifest.scriptPath.empty(), "Yaeltex package should resolve script path");
}
#endif

} // namespace

int main()
{
    testInputControllerCommands();
    testYaeltexLooperCommands();
    testYaeltexPerformanceCommands();
    testKaossPadMapping();
    testYaeltexMapping();
#if LIVELOOPING_HAS_PROFILE_IO
    testJsonProfileLoading();
    testJsonSurfaceLayoutLoading();
    testSurfaceLayoutInteractionProfile();
    testDevicePackageLoading();
#endif

    if (failures != 0) {
        std::cerr << failures << " smoke test assertion(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "core smoke tests passed\n";
    return EXIT_SUCCESS;
}
