#pragma once

#include "loop_rigger/scripting/ScriptHost.h"

#include <memory>

namespace loop_rigger::scripting {

std::unique_ptr<ScriptHost> makePythonScriptHost();

} // namespace loop_rigger::scripting
