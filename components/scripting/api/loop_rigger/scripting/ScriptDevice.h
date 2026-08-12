#pragma once

#include "loop_rigger/control/ControlMapping.h"
#include "loop_rigger/core/ControllerCommand.h"

#include <string>
#include <vector>

namespace loop_rigger::scripting {

enum class ScriptDiagnosticLevel {
    Info,
    Warning,
    Error
};

struct ScriptDiagnostic {
    ScriptDiagnosticLevel level = ScriptDiagnosticLevel::Info;
    std::string message;
};

struct ScriptEvent {
    std::string deviceId;
    std::string widgetId;
    control::WidgetEventType type = control::WidgetEventType::Press;
    float value = 1.0F;
};

struct ScriptAction {
    core::ControllerCommand command;
};

class ScriptDevice {
public:
    virtual ~ScriptDevice() = default;

    virtual std::string id() const = 0;
    virtual std::string displayName() const = 0;
    virtual std::vector<ScriptAction> handleEvent(const ScriptEvent& event) = 0;
};

std::string toString(ScriptDiagnosticLevel level);

} // namespace loop_rigger::scripting
