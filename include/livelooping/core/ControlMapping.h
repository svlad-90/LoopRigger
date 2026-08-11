#pragma once

#include "livelooping/core/ControllerCommand.h"

#include <optional>
#include <string>
#include <vector>

namespace livelooping::core {

enum class WidgetType {
    Button,
    Knob,
    Fader,
    Joystick
};

enum class WidgetEventType {
    Press,
    Release,
    Change
};

enum class MidiMessageType {
    Note,
    ControlChange
};

struct MidiEvent {
    MidiMessageType type = MidiMessageType::ControlChange;
    int channel = 0;
    int number = 0;
    int value = 0;
};

struct ControllerWidget {
    std::string id;
    std::string label;
    WidgetType type = WidgetType::Button;
};

struct WidgetEvent {
    std::string widgetId;
    WidgetEventType type = WidgetEventType::Press;
    float value = 1.0F;
};

struct MidiBinding {
    MidiMessageType type = MidiMessageType::ControlChange;
    int channel = 0;
    int number = 0;
};

struct ControlBinding {
    std::string widgetId;
    WidgetEventType widgetEventType = WidgetEventType::Press;
    std::optional<MidiBinding> midi;
    ControllerCommand command;
    bool useEventValue = false;
    bool triggerOnNonZero = true;
};

struct ControllerProfile {
    std::string id;
    std::string displayName;
    ControllerId controller = ControllerId::PseudoGui;
    std::vector<ControllerWidget> widgets;
    std::vector<ControlBinding> bindings;
};

class MidiMapper {
public:
    explicit MidiMapper(ControllerProfile profile);

    const ControllerProfile& profile() const;
    std::optional<ControllerCommand> mapMidi(const MidiEvent& event) const;
    std::optional<ControllerCommand> mapWidget(const WidgetEvent& event) const;

private:
    ControllerProfile profile_;
};

ControllerProfile makeKaossPadInputProfile(
    std::string id,
    std::string displayName,
    ControllerId controller,
    InputTarget target);

ControllerProfile makeMicKaossPadProfile();
ControllerProfile makeSynthKaossPadProfile();
ControllerProfile makeYaeltexLiveLoopingProfile();

float normalizeMidiValue(int value);

} // namespace livelooping::core

