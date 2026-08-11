#pragma once

#include <string>
#include <vector>

namespace livelooping::profile {

struct SurfaceBounds {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
};

enum class SurfaceElementRole {
    Decoration,
    Widget
};

enum class SurfaceElementShape {
    Rect,
    RoundRect,
    Circle,
    Text,
    Line,
    Knob,
    Fader,
    Joystick
};

struct SurfaceElement {
    std::string id;
    SurfaceElementRole role = SurfaceElementRole::Decoration;
    SurfaceElementShape shape = SurfaceElementShape::Rect;
    std::string widgetId;
    std::string label;
    std::string group;
    SurfaceBounds bounds;
};

struct ControlSurfaceLayout {
    std::string id;
    std::string profileId;
    int baseWidth = 0;
    int baseHeight = 0;
    std::vector<SurfaceElement> elements;
};

} // namespace livelooping::profile
