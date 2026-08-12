#pragma once

#include "loop_rigger/scripting/ScriptDevice.h"

#include <memory>
#include <string>
#include <vector>

namespace loop_rigger::scripting {

struct ScriptLoadResult {
    std::shared_ptr<ScriptDevice> device;
    std::vector<ScriptDiagnostic> diagnostics;
};

class ScriptHost {
public:
    virtual ~ScriptHost() = default;

    virtual ScriptLoadResult loadDeviceScript(const std::string& scriptPath) = 0;
};

} // namespace loop_rigger::scripting
