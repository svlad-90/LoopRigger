#pragma once

#include "loop_rigger/plugin_host/PluginScanner.h"

#include <memory>

namespace loop_rigger::plugin_host {

std::unique_ptr<PluginScanner> makeJucePluginScanner();

} // namespace loop_rigger::plugin_host
