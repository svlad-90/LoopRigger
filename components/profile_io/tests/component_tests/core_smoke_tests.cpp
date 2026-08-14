#include "loop_rigger/control/ControlMapping.h"
#include "loop_rigger/core/LiveLoopingEngine.h"

#if LIVELOOPING_HAS_PROFILE_IO
#include "loop_rigger/profile_io/ProfileLoader.h"
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

    const auto yaeltexLayout = loadControlSurfaceLayoutFromFile(layoutPath("yaeltex_livelooping.json"));
    expect(yaeltexLayout.id == "yaeltex_livelooping", "Yaeltex layout should load id");
    expect(yaeltexLayout.profileId == "yaeltex.livelooping", "Yaeltex layout should target Yaeltex profile");
    expect(yaeltexLayout.baseWidth == 1520, "Yaeltex layout should expose base width");
    expect(yaeltexLayout.baseHeight == 900, "Yaeltex layout should expose base height");
    expect(yaeltexLayout.elements.size() >= 80, "Yaeltex layout should include dense faceplate elements");

    const auto yaeltexProfile = loadControllerProfileFromFile(profilePath("yaeltex_livelooping.json"));
    expectLayoutElementsStayInsideCanvas(yaeltexLayout);
    expectLayoutWidgetsBelongToProfile(yaeltexLayout, yaeltexProfile);
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
    testDevicePackageLoading();
#endif

    if (failures != 0) {
        std::cerr << failures << " smoke test assertion(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "core smoke tests passed\n";
    return EXIT_SUCCESS;
}
