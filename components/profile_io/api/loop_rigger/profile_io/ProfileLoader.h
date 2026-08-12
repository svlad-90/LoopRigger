#pragma once

#include "loop_rigger/control/ControlMapping.h"
#include "loop_rigger/profile_io/SurfaceLayout.h"

#include <string>

namespace loop_rigger::profile_io {

control::ControllerProfile loadControllerProfileFromFile(const std::string& path);
control::ControllerProfile loadControllerProfileFromJson(const std::string& jsonText);
ControlSurfaceLayout loadControlSurfaceLayoutFromFile(const std::string& path);
ControlSurfaceLayout loadControlSurfaceLayoutFromJson(const std::string& jsonText);

} // namespace loop_rigger::profile_io
