#include "loop_rigger/scripting/ScriptDevice.h"

namespace loop_rigger::scripting {

std::string toString(ScriptDiagnosticLevel level)
{
    switch (level) {
    case ScriptDiagnosticLevel::Info:
        return "info";
    case ScriptDiagnosticLevel::Warning:
        return "warning";
    case ScriptDiagnosticLevel::Error:
        return "error";
    }
    return "unknown";
}

} // namespace loop_rigger::scripting
