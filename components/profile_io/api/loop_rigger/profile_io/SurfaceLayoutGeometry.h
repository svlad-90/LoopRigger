#pragma once

#include "loop_rigger/profile_io/SurfaceLayout.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace loop_rigger::profile_io {

struct SurfaceGroupGeometry {
    std::string group;
    SurfaceBounds bounds;
    size_t elementCount = 0;
    size_t widgetCount = 0;
    size_t decorationCount = 0;
};

struct SurfaceLayoutProfile {
    size_t elementCount = 0;
    size_t widgetCount = 0;
    size_t decorationCount = 0;
};

SurfaceLayoutProfile profileSurfaceLayout(const ControlSurfaceLayout& layout);
std::vector<SurfaceGroupGeometry> summarizeSurfaceGroups(const ControlSurfaceLayout& layout);
std::optional<SurfaceGroupGeometry> findSurfaceGroupGeometry(const ControlSurfaceLayout& layout, const std::string& group);
bool containsSurfaceBounds(const SurfaceBounds& outer, const SurfaceBounds& inner);
bool surfaceBoundsOverlap(const SurfaceBounds& lhs, const SurfaceBounds& rhs);

} // namespace loop_rigger::profile_io
