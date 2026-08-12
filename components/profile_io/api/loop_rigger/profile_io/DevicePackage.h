#pragma once

#include "loop_rigger/control/ControlMapping.h"
#include "loop_rigger/profile_io/SurfaceLayout.h"

#include <string>

namespace loop_rigger::profile_io {

struct DevicePackageManifest {
    std::string id;
    std::string displayName;
    std::string rootPath;
    std::string controllerProfilePath;
    std::string controlSurfaceLayoutPath;
    std::string scriptPath;
};

struct LoadedDevicePackage {
    DevicePackageManifest manifest;
    control::ControllerProfile controllerProfile;
    ControlSurfaceLayout controlSurfaceLayout;
};

bool hasScript(const DevicePackageManifest& manifest);

} // namespace loop_rigger::profile_io
