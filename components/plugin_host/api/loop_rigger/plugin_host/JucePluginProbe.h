#pragma once

#include "loop_rigger/plugin_host/PluginProbe.h"

#include <memory>

namespace loop_rigger::plugin_host {

std::unique_ptr<PluginProber> makeJucePluginProber();

} // namespace loop_rigger::plugin_host
