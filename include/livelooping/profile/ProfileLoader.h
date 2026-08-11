#pragma once

#include "livelooping/core/ControlMapping.h"

#include <string>

namespace livelooping::profile {

core::ControllerProfile loadControllerProfileFromFile(const std::string& path);
core::ControllerProfile loadControllerProfileFromJson(const std::string& jsonText);

} // namespace livelooping::profile
