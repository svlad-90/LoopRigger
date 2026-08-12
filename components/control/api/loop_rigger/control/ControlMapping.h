#pragma once

#include "loop_rigger/core/ControllerCommand.h"

#include <optional>
#include <string>
#include <vector>

namespace loop_rigger::control {

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
    std::string group;
    int row = 0;
    int column = 0;
    int width = 1;
    int height = 1;
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
    core::ControllerCommand command;
    bool useEventValue = false;
    bool triggerOnNonZero = true;
};

struct ControllerProfile {
    std::string id;
    std::string displayName;
    core::ControllerId controller = core::ControllerId::PseudoGui;
    std::vector<ControllerWidget> widgets;
    std::vector<ControlBinding> bindings;
};

class MidiMapper {
public:
    explicit MidiMapper(ControllerProfile profile);

    const ControllerProfile& profile() const;
    std::optional<core::ControllerCommand> mapMidi(const MidiEvent& event) const;
    std::optional<core::ControllerCommand> mapWidget(const WidgetEvent& event) const;

private:
    ControllerProfile profile_;
};

ControllerProfile makeKaossPadInputProfile(
    std::string id,
    std::string displayName,
    core::ControllerId controller,
    core::InputTarget target);

ControllerProfile makeMicKaossPadProfile();
ControllerProfile makeSynthKaossPadProfile();
ControllerProfile makeYaeltexLiveLoopingProfile();

float normalizeMidiValue(int value);
std::string toString(WidgetType type);

} // namespace loop_rigger::control
