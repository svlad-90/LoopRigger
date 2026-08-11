#include "livelooping/core/ControlMapping.h"

#include <algorithm>
#include <utility>

namespace livelooping::core {

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

std::optional<ControllerCommand> MidiMapper::mapMidi(const MidiEvent& event) const
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

std::optional<ControllerCommand> MidiMapper::mapWidget(const WidgetEvent& event) const
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

    profile.widgets.push_back(knob("fx_level", "FX level", "levels", 0, 1));
    profile.bindings.push_back(widgetBinding(
        "fx_level",
        CommandType::SetInputFxLevel,
        0,
        0.0F,
        target,
        true));

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
            MidiMessageType::Note,
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
            MidiMessageType::Note,
            0,
            72 + i,
            CommandType::SelectSampleLength,
            lengths[i]));
    }

    for (int track = 0; track < 4; ++track) {
        profile.widgets.push_back(button("record_t" + std::to_string(track + 1), "Record T" + std::to_string(track + 1), "track_record", 0, track));
        profile.bindings.push_back(midiBinding(
            "record_t" + std::to_string(track + 1),
            MidiMessageType::Note,
            0,
            80 + track,
            CommandType::ToggleTrackRecording,
            track));

        profile.widgets.push_back(button("clear_t" + std::to_string(track + 1), "Clear T" + std::to_string(track + 1), "track_clear", 0, track));
        profile.bindings.push_back(midiBinding(
            "clear_t" + std::to_string(track + 1),
            MidiMessageType::Note,
            0,
            84 + track,
            CommandType::ClearTrack,
            track));

        profile.widgets.push_back(knob("vol_pan_t" + std::to_string(track + 1), "Vol/Pan T" + std::to_string(track + 1), "track_volume_pan", 0, track));
        profile.bindings.push_back(widgetBinding(
            "vol_pan_t" + std::to_string(track + 1),
            CommandType::SetTrackVolume,
            track,
            0.0F,
            InputTarget::Mic,
            true));
    }

    profile.widgets.push_back(button("resample_selected", "Resample L", "resampling", 0, 0));
    profile.bindings.push_back(midiBinding(
        "resample_selected",
        MidiMessageType::Note,
        0,
        90,
        CommandType::StartResampleSelectedLooper));

    profile.widgets.push_back(button("resample_all", "Resample all", "resampling", 0, 1));
    profile.bindings.push_back(midiBinding(
        "resample_all",
        MidiMessageType::Note,
        0,
        91,
        CommandType::StartResampleAllLoopers));

    profile.widgets.push_back(button("reset_all", "Clear", "session", 0, 1));
    profile.bindings.push_back(midiBinding(
        "reset_all",
        MidiMessageType::Note,
        0,
        92,
        CommandType::ResetAll));

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

} // namespace livelooping::core
