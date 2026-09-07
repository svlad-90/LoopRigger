#include "loop_rigger/control/ControlMapping.h"
#include "loop_rigger/core/EngineState.h"

#include <algorithm>
#include <utility>

namespace loop_rigger::control {

using core::CommandType;
using core::ControllerCommand;
using core::ControllerId;
using core::InputTarget;
using core::kRemixerModes;

namespace {

ControllerWidget widget(
    std::string id,
    std::string label,
    WidgetType type,
    std::string group,
    int row,
    int column,
    int width = 1,
    int height = 1)
{
    ControllerWidget result;
    result.id = std::move(id);
    result.label = std::move(label);
    result.type = type;
    result.group = std::move(group);
    result.row = row;
    result.column = column;
    result.width = width;
    result.height = height;
    return result;
}

ControllerWidget button(std::string id, std::string label, std::string group, int row, int column, int width = 1)
{
    return widget(std::move(id), std::move(label), WidgetType::Button, std::move(group), row, column, width);
}

ControllerWidget knob(std::string id, std::string label, std::string group, int row, int column)
{
    return widget(std::move(id), std::move(label), WidgetType::Knob, std::move(group), row, column);
}

ControlBinding widgetBinding(
    std::string widgetId,
    CommandType commandType,
    int index = 0,
    float value = 0.0F,
    InputTarget target = InputTarget::Mic,
    bool useEventValue = false)
{
    ControllerCommand command;
    command.type = commandType;
    command.inputTarget = target;
    command.index = index;
    command.value = value;

    ControlBinding binding;
    binding.widgetId = std::move(widgetId);
    binding.widgetEventType = useEventValue ? WidgetEventType::Change : WidgetEventType::Press;
    binding.command = command;
    binding.useEventValue = useEventValue;
    return binding;
}

ControlBinding midiBinding(
    std::string widgetId,
    MidiMessageType type,
    int channel,
    int number,
    CommandType commandType,
    int index = 0,
    InputTarget target = InputTarget::Mic,
    bool useEventValue = false)
{
    auto binding = widgetBinding(std::move(widgetId), commandType, index, 0.0F, target, useEventValue);
    binding.midi = MidiBinding{type, channel, number};
    return binding;
}

void assignController(ControllerProfile& profile)
{
    for (auto& binding : profile.bindings) {
        binding.command.controller = profile.controller;
    }
}

} // namespace

MidiMapper::MidiMapper(ControllerProfile profile)
    : profile_(std::move(profile))
{
}

const ControllerProfile& MidiMapper::profile() const
{
    return profile_;
}

std::optional<core::ControllerCommand> MidiMapper::mapMidi(const MidiEvent& event) const
{
    for (const auto& binding : profile_.bindings) {
        if (!binding.midi.has_value()) {
            continue;
        }
        const auto& midi = binding.midi.value();
        if (midi.type != event.type || midi.channel != event.channel || midi.number != event.number) {
            continue;
        }
        if (binding.triggerOnNonZero && event.value == 0) {
            return std::nullopt;
        }

        auto command = binding.command;
        if (binding.useEventValue) {
            command.value = normalizeMidiValue(event.value);
        }
        return command;
    }
    return std::nullopt;
}

std::optional<core::ControllerCommand> MidiMapper::mapWidget(const WidgetEvent& event) const
{
    for (const auto& binding : profile_.bindings) {
        if (binding.widgetId != event.widgetId || binding.widgetEventType != event.type) {
            continue;
        }
        if (binding.triggerOnNonZero && event.value == 0.0F) {
            return std::nullopt;
        }

        auto command = binding.command;
        if (binding.useEventValue) {
            command.value = std::clamp(event.value, 0.0F, 1.0F);
        }
        return command;
    }
    return std::nullopt;
}

ControllerProfile makeKaossPadInputProfile(
    std::string id,
    std::string displayName,
    ControllerId controller,
    InputTarget target)
{
    ControllerProfile profile;
    profile.id = std::move(id);
    profile.displayName = std::move(displayName);
    profile.controller = controller;

    for (int preset = 0; preset < 8; ++preset) {
        profile.widgets.push_back(button("preset_" + std::to_string(preset + 1), std::to_string(preset + 1), "presets", 0, preset));
        profile.bindings.push_back(midiBinding(
            "preset_" + std::to_string(preset + 1),
            MidiMessageType::ControlChange,
            0,
            49 + preset,
            CommandType::SelectInputPreset,
            preset,
            target));
    }

    for (int page = 0; page < 4; ++page) {
        profile.widgets.push_back(button("page_" + std::to_string(page + 1), "Page " + std::to_string(page + 1), "pages", 0, page));
        profile.bindings.push_back(widgetBinding(
            "page_" + std::to_string(page + 1),
            CommandType::SelectInputPresetPage,
            page,
            0.0F,
            target));
    }

    profile.widgets.push_back(knob("input_volume", "Input volume", "levels", 0, 0));
    profile.bindings.push_back(midiBinding(
        "input_volume",
        MidiMessageType::ControlChange,
        0,
        93,
        CommandType::SetInputVolume,
        0,
        target,
        true));
    profile.widgets.push_back(knob("input_volume_knob", "Input volume", "levels", 0, 0));
    profile.bindings.push_back(widgetBinding(
        "input_volume_knob",
        CommandType::SetInputVolume,
        0,
        0.0F,
        target,
        true));
    profile.widgets.push_back(widget("input_volume_fader", "Input volume", WidgetType::Fader, "levels", 1, 0));
    profile.bindings.push_back(widgetBinding(
        "input_volume_fader",
        CommandType::SetInputVolume,
        0,
        0.0F,
        target,
        true));

    profile.widgets.push_back(knob("fx_level", "FX level", "levels", 0, 1));
    profile.bindings.push_back(widgetBinding(
        "fx_level",
        CommandType::SetInputFxLevel,
        0,
        0.0F,
        target,
        true));
    profile.widgets.push_back(knob("fx_level_knob", "FX level", "levels", 0, 1));
    profile.bindings.push_back(widgetBinding(
        "fx_level_knob",
        CommandType::SetInputFxLevel,
        0,
        0.0F,
        target,
        true));
    profile.widgets.push_back(widget("fx_level_fader", "FX level", WidgetType::Fader, "levels", 1, 1));
    profile.bindings.push_back(widgetBinding(
        "fx_level_fader",
        CommandType::SetInputFxLevel,
        0,
        0.0F,
        target,
        true));

    profile.widgets.push_back(button("hold", "HOLD", "hold", 0, 0));
    profile.bindings.push_back(widgetBinding(
        "hold",
        CommandType::ToggleInputFxHold,
        0,
        0.0F,
        target));

    for (int parameter = 0; parameter < 8; ++parameter) {
        profile.widgets.push_back(knob("fx_parameter_" + std::to_string(parameter + 1), "FX " + std::to_string(parameter + 1), "fx_parameters", 0, parameter));
        profile.bindings.push_back(midiBinding(
            "fx_parameter_" + std::to_string(parameter + 1),
            MidiMessageType::ControlChange,
            0,
            70 + parameter,
            CommandType::SetInputFxParameter,
            parameter,
            target,
            true));
    }

    assignController(profile);
    return profile;
}

ControllerProfile makeMicKaossPadProfile()
{
    return makeKaossPadInputProfile("kaoss.mic", "Mic Kaoss Pad", ControllerId::MicKaossPad, InputTarget::Mic);
}

ControllerProfile makeSynthKaossPadProfile()
{
    return makeKaossPadInputProfile("kaoss.synth", "Synth Kaoss Pad", ControllerId::SynthKaossPad, InputTarget::Synth);
}

ControllerProfile makeYaeltexLiveLoopingProfile()
{
    ControllerProfile profile;
    profile.id = "yaeltex.livelooping";
    profile.displayName = "Yaeltex LiveLooping";
    profile.controller = ControllerId::Yaeltex;

    for (int looper = 0; looper < 4; ++looper) {
        profile.widgets.push_back(button("looper_" + std::to_string(looper + 1), "Looper " + std::to_string(looper + 1), "looper_select", 0, looper));
        profile.bindings.push_back(midiBinding(
            "looper_" + std::to_string(looper + 1),
            MidiMessageType::ControlChange,
            0,
            60 + looper,
            CommandType::SelectLooper,
            looper));
    }

    const int lengths[] = {1, 2, 4, 8, 16, 32, 64, 128};
    for (int i = 0; i < 8; ++i) {
        profile.widgets.push_back(button("sample_length_" + std::to_string(lengths[i]), std::to_string(lengths[i]), "sample_length", i / 4, i % 4));
        profile.bindings.push_back(midiBinding(
            "sample_length_" + std::to_string(lengths[i]),
            MidiMessageType::ControlChange,
            0,
            72 + i,
            CommandType::SelectSampleLength,
            lengths[i]));
    }

    const char* clockDivisionLabels[] = {"4", "2", "1", "1/2", "1/4", "1/8", "1/16", "1/32"};
    for (int i = 0; i < 8; ++i) {
        profile.widgets.push_back(button("top_grid_" + std::to_string(i + 1), clockDivisionLabels[i], "clock_division", i / 4, i % 4));
        profile.bindings.push_back(midiBinding(
            "top_grid_" + std::to_string(i + 1),
            MidiMessageType::ControlChange,
            0,
            36 + i,
            CommandType::SelectClockDivision,
            i));
    }

    for (int i = 0; i < 16; ++i) {
        const auto isInvert = i >= 8;
        const auto local = i % 8;
        const auto isLooperTarget = local >= 4;
        const auto index = local % 4;
        const auto widgetId = "mute_" + std::to_string(i + 1);
        const auto label = std::string(isInvert ? "Inv " : "Mute ") + (isLooperTarget ? "L" : "T") + std::to_string(index + 1);
        const auto commandType = isInvert
            ? (isLooperTarget ? CommandType::ToggleLooperInvert : CommandType::ToggleTrackInvert)
            : (isLooperTarget ? CommandType::ToggleLooperMute : CommandType::ToggleTrackMute);
        profile.widgets.push_back(button(widgetId, label, "mute_invert", i / 8, local));
        const auto midiNumber = isInvert
            ? (isLooperTarget ? 56 + index : 48 + index)
            : (isLooperTarget ? 52 + index : 44 + index);
        profile.bindings.push_back(midiBinding(widgetId, MidiMessageType::ControlChange, 0, midiNumber, commandType, index));
    }

    for (int track = 0; track < 4; ++track) {
        profile.widgets.push_back(button("record_t" + std::to_string(track + 1), "Record T" + std::to_string(track + 1), "track_record", 0, track));
        profile.bindings.push_back(midiBinding(
            "record_t" + std::to_string(track + 1),
            MidiMessageType::ControlChange,
            1,
            8 + track,
            CommandType::ToggleTrackRecording,
            track));

        profile.widgets.push_back(button("clear_t" + std::to_string(track + 1), "Clear T" + std::to_string(track + 1), "track_clear", 0, track));
        profile.bindings.push_back(midiBinding(
            "clear_t" + std::to_string(track + 1),
            MidiMessageType::ControlChange,
            1,
            12 + track,
            CommandType::ClearTrack,
            track));

        profile.widgets.push_back(knob("vol_pan_t" + std::to_string(track + 1), "Vol/Pan T" + std::to_string(track + 1), "track_volume_pan", 0, track));
        profile.bindings.push_back(midiBinding(
            "vol_pan_t" + std::to_string(track + 1),
            MidiMessageType::ControlChange,
            0,
            16 + track,
            CommandType::SetTrackVolume,
            track,
            InputTarget::Mic,
            true));
        profile.bindings.push_back(midiBinding(
            "vol_pan_t" + std::to_string(track + 1),
            MidiMessageType::ControlChange,
            2,
            16 + track,
            CommandType::SetTrackPan,
            track,
            InputTarget::Mic,
            true));

        profile.widgets.push_back(button("select_t" + std::to_string(track + 1), "Select T" + std::to_string(track + 1), "track_select", 0, track));
        profile.bindings.push_back(midiBinding(
            "select_t" + std::to_string(track + 1),
            MidiMessageType::ControlChange,
            1,
            16 + track,
            CommandType::ToggleTrackSelection,
            track));
    }

    for (int looper = 0; looper < 4; ++looper) {
        profile.widgets.push_back(knob("vol_pan_l" + std::to_string(looper + 1), "Vol/Pan L" + std::to_string(looper + 1), "looper_volume_pan", 0, looper));
        profile.bindings.push_back(midiBinding(
            "vol_pan_l" + std::to_string(looper + 1),
            MidiMessageType::ControlChange,
            0,
            24 + looper,
            CommandType::SetLooperVolume,
            looper,
            InputTarget::Mic,
            true));
    }

    profile.widgets.push_back(button("resample_selected", "Resample L", "resampling", 0, 0));
    profile.bindings.push_back(widgetBinding("resample_selected", CommandType::StartResampleSelectedLooper));

    profile.widgets.push_back(button("resample_all", "Resample all", "resampling", 0, 1));
    profile.bindings.push_back(widgetBinding("resample_all", CommandType::StartResampleAllLoopers));

    profile.widgets.push_back(button("reset_all", "Clear", "session", 0, 1));
    profile.bindings.push_back(midiBinding(
        "reset_all",
        MidiMessageType::ControlChange,
        0,
        33,
        CommandType::ResetAll));

    profile.widgets.push_back(button("transport_start", "Start", "session", 0, 0));
    profile.bindings.push_back(midiBinding("transport_start", MidiMessageType::ControlChange, 0, 32, CommandType::StartTransport));

    profile.widgets.push_back(button("transport_stop", "Stop", "session", 0, 2));
    profile.bindings.push_back(widgetBinding("transport_stop", CommandType::StopTransport));

    profile.widgets.push_back(button("restart_all_loopers", "Restart all", "session", 0, 3));
    profile.bindings.push_back(widgetBinding("restart_all_loopers", CommandType::RestartAllLoopers));

    const char* routingSourceIds[] = {
        "routing_mic",
        "routing_synth",
        "routing_looper",
        "routing_looper_2",
        "routing_looper_3",
        "routing_looper_4",
        "routing_looper_all",
        "routing_recording_bus",
    };
    const char* routingSourceLabels[] = {
        "MIC",
        "SYNTH",
        "LOOPER",
        "LOOPER 2",
        "LOOPER 3",
        "LOOPER 4",
        "LOOPER ALL",
        "RECORD",
    };
    const int routingSourceIndexes[] = {0, 1, 3, 4, 5, 6, 7, 8};
    const int routingSourceMidiNumbers[] = {64, 65, 68, 69, 70, 71, 66, 67};
    for (int i = 0; i < 8; ++i) {
        profile.widgets.push_back(button(routingSourceIds[i], routingSourceLabels[i], "routing_source", i / 4, i % 4));
        profile.bindings.push_back(midiBinding(
            routingSourceIds[i],
            MidiMessageType::ControlChange,
            0,
            routingSourceMidiNumbers[i],
            CommandType::SelectRoutingSource,
            routingSourceIndexes[i]));
    }

    for (int slot = 0; slot < 10; ++slot) {
        profile.widgets.push_back(button("center_fx_slot_" + std::to_string(slot + 1), "FX" + std::to_string(slot + 1), "center_fx_slot", slot / 5, slot % 5));
        const auto midiNumber = slot < 5 ? 88 + slot : 96 + (slot - 5);
        profile.bindings.push_back(midiBinding(
            "center_fx_slot_" + std::to_string(slot + 1),
            MidiMessageType::ControlChange,
            0,
            midiNumber,
            CommandType::SelectCenterFxSlot,
            slot));
    }

    for (int bank = 0; bank < 5; ++bank) {
        profile.widgets.push_back(button("center_fx_bank_" + std::to_string(bank + 1), "B" + std::to_string(bank + 1), "center_fx_bank", bank / 3, bank % 3));
        const auto midiNumber = bank < 3 ? 93 + bank : 101 + (bank - 3);
        profile.bindings.push_back(midiBinding(
            "center_fx_bank_" + std::to_string(bank + 1),
            MidiMessageType::ControlChange,
            0,
            midiNumber,
            CommandType::SelectCenterFxBank,
            bank));
    }

    const char* centerFxParameterIds[] = {"center_fx_dry_wet", "center_fx_lfo1_speed", "center_fx_lfo2_speed", "center_fx_drop"};
    const char* centerFxParameterLabels[] = {"Dry/Wet", "LFO1 Speed", "LFO2 Speed", "Drop FX"};
    for (int parameter = 0; parameter < 4; ++parameter) {
        profile.widgets.push_back(knob(centerFxParameterIds[parameter], centerFxParameterLabels[parameter], "center_fx_parameter", 0, parameter));
        profile.bindings.push_back(midiBinding(
            centerFxParameterIds[parameter],
            MidiMessageType::ControlChange,
            0,
            4 + parameter,
            CommandType::SetCenterFxParameter,
            parameter,
            InputTarget::Mic,
            true));
    }

    const char* topFxParameterIds[] = {"top_vol_drop", "top_reverb", "top_transpose", "top_phaser"};
    const char* topFxParameterLabels[] = {"Vol/Drop", "Reverb", "Transpose", "Phaser"};
    for (int parameter = 0; parameter < 4; ++parameter) {
        profile.widgets.push_back(knob(topFxParameterIds[parameter], topFxParameterLabels[parameter], "top_fx_parameter", 0, parameter));
        profile.bindings.push_back(midiBinding(
            topFxParameterIds[parameter],
            MidiMessageType::ControlChange,
            0,
            parameter,
            CommandType::SetTopFxParameter,
            parameter,
            InputTarget::Mic,
            true));
    }

    for (int joystick = 0; joystick < 2; ++joystick) {
        profile.widgets.push_back(widget("center_fx_joystick_" + std::to_string(joystick + 1), "Value " + std::to_string(joystick + 1), WidgetType::Joystick, "center_fx_joystick", 0, joystick, 2, 2));
        profile.bindings.push_back(midiBinding(
            "center_fx_joystick_" + std::to_string(joystick + 1),
            MidiMessageType::ControlChange,
            7,
            32 + (joystick * 2),
            CommandType::SetCenterFxJoystick,
            joystick,
            InputTarget::Mic,
            true));
    }

    const char* remixerMacroIds[] = {
        "remixer_freeze",
        "remixer_drop",
        "remixer_extra_1",
        "remixer_record",
        "remixer_reset_current",
        "remixer_reset_all",
        "remixer_extra_2",
        "remixer_stop",
    };
    const char* remixerMacroLabels[] = {"FREEZE\ncurrent", "Drop", "Extra", "SEQ REC", "RESET\ncurrent", "RESET\nall", "Extra 2", "STOP\nSEQ REC"};
    for (int macro = 0; macro < 8; ++macro) {
        profile.widgets.push_back(button(remixerMacroIds[macro], remixerMacroLabels[macro], "remixer_macro", macro / 4, macro % 4));
        profile.bindings.push_back(widgetBinding(remixerMacroIds[macro], CommandType::TriggerRemixerMacro, macro));
    }

    const char* remixerModeLabels[] = {
        "GATE\ncurrent",
        "GATE\nall",
        "Extra 3",
        "REVERSE",
        "CTRL all",
        "min/max",
        "Nat.",
        "Reverb",
        "I",
        "V",
        "Harm.",
        "Delay",
        "II",
        "VI",
        "Melod.",
        "Phaser",
        "III",
        "VII",
        "",
        "",
        "IV",
        "VIII",
        "MIDI\nSCALE",
        "CLEAR",
    };
    for (int mode = 0; mode < kRemixerModes; ++mode) {
        profile.widgets.push_back(button("remixer_mode_" + std::to_string(mode + 1), remixerModeLabels[mode], "remixer_mode", mode / 4, mode % 4));
        profile.bindings.push_back(midiBinding(
            "remixer_mode_" + std::to_string(mode + 1),
            MidiMessageType::ControlChange,
            0,
            104 + mode,
            CommandType::TriggerRemixerMode,
            mode));
    }

    for (int slot = 0; slot < 8; ++slot) {
        profile.widgets.push_back(button("animation_" + std::to_string(slot + 1), "Ani. " + std::to_string(slot + 1), "animation_slot", 0, slot));
        profile.bindings.push_back(midiBinding(
            "animation_" + std::to_string(slot + 1),
            MidiMessageType::ControlChange,
            1,
            20 + slot,
            CommandType::TriggerAnimationSlot,
            slot));
    }

    const char* presetSlotIds[] = {"preset_on_off", "preset_default", "preset_pr1", "preset_pr2", "preset_pr3", "preset_pr4", "preset_pr5", "preset_pr6"};
    const char* presetSlotLabels[] = {"On/Off", "Default", "Pr.1", "Pr.2", "Pr.3", "Pr.4", "Pr.5", "Pr.6"};
    for (int slot = 0; slot < 8; ++slot) {
        profile.widgets.push_back(button(presetSlotIds[slot], presetSlotLabels[slot], "preset_slot", 0, slot));
        profile.bindings.push_back(widgetBinding(presetSlotIds[slot], CommandType::TriggerPresetSlot, slot));
    }

    for (int track = 0; track < 4; ++track) {
        profile.widgets.push_back(knob("sidechain_stash_t" + std::to_string(track + 1), "Sidechain Vol/Stash T" + std::to_string(track + 1), "sidechain_parameter", 0, track));
        profile.bindings.push_back(midiBinding(
            "sidechain_stash_t" + std::to_string(track + 1),
            MidiMessageType::ControlChange,
            0,
            20 + track,
            CommandType::SetSidechainParameter,
            track,
            InputTarget::Mic,
            true));

        profile.widgets.push_back(knob("sidechain_decay_t" + std::to_string(track + 1), "Sidechain Dec/Ten T" + std::to_string(track + 1), "sidechain_parameter", 1, track));
        profile.bindings.push_back(midiBinding(
            "sidechain_decay_t" + std::to_string(track + 1),
            MidiMessageType::ControlChange,
            0,
            28 + track,
            CommandType::SetSidechainParameter,
            4 + track,
            InputTarget::Mic,
            true));
    }

    profile.widgets.push_back(button("bottom_record", "Record", "bottom_record", 0, 0));
    profile.bindings.push_back(widgetBinding("bottom_record", CommandType::ToggleTrackRecording, 0));

    profile.widgets.push_back(button("bottom_extra_clear_l", "Extra/Clear L", "bottom_extra_clear", 0, 0));
    profile.bindings.push_back(widgetBinding("bottom_extra_clear_l", CommandType::ResetLooper));

    const char* masterParameterIds[] = {
        "master_tempo",
        "master_pitch_shift",
        "master_volume",
        "master_pitch_dry_wet",
        "master_output_volume",
        "master_distortion",
        "master_pan",
        "master_distortion_dry_wet",
    };
    const char* masterParameterLabels[] = {
        "Master tempo",
        "Pitch shift",
        "Master vol.",
        "Pitch dry/wet",
        "VOLUME",
        "Distortion",
        "Pan",
        "Dist dry/wet",
    };
    const int masterParameterMidiNumbers[] = {8, 12, 10, 14, 9, 13, 11, 15};
    for (int parameter = 0; parameter < 8; ++parameter) {
        profile.widgets.push_back(knob(masterParameterIds[parameter], masterParameterLabels[parameter], "master_parameter", parameter / 2, parameter % 2));
        profile.bindings.push_back(midiBinding(
            masterParameterIds[parameter],
            MidiMessageType::ControlChange,
            0,
            masterParameterMidiNumbers[parameter],
            CommandType::SetMasterParameter,
            parameter,
            InputTarget::Mic,
            true));
    }

    for (int slot = 0; slot < 8; ++slot) {
        profile.widgets.push_back(button("sampler_slot_" + std::to_string(slot + 1), std::to_string(slot + 1), "sampler", slot / 4, slot % 4));
        profile.bindings.push_back(midiBinding(
            "sampler_slot_" + std::to_string(slot + 1),
            MidiMessageType::ControlChange,
            1,
            40 + slot,
            CommandType::TriggerSamplerSlot,
            slot));
    }

    assignController(profile);
    return profile;
}

float normalizeMidiValue(int value)
{
    const auto clamped = std::clamp(value, 0, 127);
    return static_cast<float>(clamped) / 127.0F;
}

std::string toString(WidgetType type)
{
    switch (type) {
    case WidgetType::Button:
        return "button";
    case WidgetType::Knob:
        return "knob";
    case WidgetType::Fader:
        return "fader";
    case WidgetType::Joystick:
        return "joystick";
    }
    return "unknown";
}

} // namespace loop_rigger::control
