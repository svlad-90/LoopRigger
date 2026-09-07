#include "loop_rigger/profile_io/SurfaceLayoutGeometry.h"

#include <algorithm>
#include <map>

namespace loop_rigger::profile_io {

namespace {

float rightEdge(const SurfaceBounds& bounds)
{
    return bounds.x + bounds.width;
}

float bottomEdge(const SurfaceBounds& bounds)
{
    return bounds.y + bounds.height;
}

void expandToInclude(SurfaceBounds& target, const SurfaceBounds& bounds)
{
    const auto left = std::min(target.x, bounds.x);
    const auto top = std::min(target.y, bounds.y);
    const auto right = std::max(rightEdge(target), rightEdge(bounds));
    const auto bottom = std::max(bottomEdge(target), bottomEdge(bounds));

    target.x = left;
    target.y = top;
    target.width = right - left;
    target.height = bottom - top;
}

SurfaceBounds expandedBounds(const SurfaceBounds& bounds, float horizontal, float vertical)
{
    return {
        bounds.x - horizontal,
        bounds.y - vertical,
        bounds.width + horizontal * 2.0F,
        bounds.height + vertical * 2.0F,
    };
}

SurfaceBounds expandedBounds(const SurfaceBounds& bounds, float left, float top, float right, float bottom)
{
    return {
        bounds.x - left,
        bounds.y - top,
        bounds.width + left + right,
        bounds.height + top + bottom,
    };
}

} // namespace

SurfaceLayoutProfile profileSurfaceLayout(const ControlSurfaceLayout& layout)
{
    SurfaceLayoutProfile profile;
    profile.elementCount = layout.elements.size();
    for (const auto& element : layout.elements) {
        if (element.role == SurfaceElementRole::Widget) {
            ++profile.widgetCount;
        } else {
            ++profile.decorationCount;
        }
    }
    return profile;
}

std::vector<SurfaceGroupGeometry> summarizeSurfaceGroups(const ControlSurfaceLayout& layout)
{
    std::map<std::string, SurfaceGroupGeometry> groups;

    for (const auto& element : layout.elements) {
        if (element.group.empty()) {
            continue;
        }

        auto [iterator, inserted] = groups.try_emplace(element.group);
        auto& geometry = iterator->second;
        if (inserted) {
            geometry.group = element.group;
            geometry.bounds = element.bounds;
        } else {
            expandToInclude(geometry.bounds, element.bounds);
        }

        ++geometry.elementCount;
        if (element.role == SurfaceElementRole::Widget) {
            ++geometry.widgetCount;
        } else {
            ++geometry.decorationCount;
        }
    }

    std::vector<SurfaceGroupGeometry> result;
    result.reserve(groups.size());
    for (const auto& [_, geometry] : groups) {
        result.push_back(geometry);
    }
    return result;
}

std::optional<SurfaceGroupGeometry> findSurfaceGroupGeometry(const ControlSurfaceLayout& layout, const std::string& group)
{
    for (const auto& geometry : summarizeSurfaceGroups(layout)) {
        if (geometry.group == group) {
            return geometry;
        }
    }
    return std::nullopt;
}

bool containsSurfaceBounds(const SurfaceBounds& outer, const SurfaceBounds& inner)
{
    return inner.x >= outer.x
        && inner.y >= outer.y
        && rightEdge(inner) <= rightEdge(outer)
        && bottomEdge(inner) <= bottomEdge(outer);
}

SurfaceBounds visualSurfaceBounds(const SurfaceElement& element)
{
    if (element.role != SurfaceElementRole::Widget) {
        return element.bounds;
    }

    switch (element.shape) {
    case SurfaceElementShape::RoundRect:
        if (element.variant == "hardware_button" || element.variant == "interactive_button" || element.variant == "kaoss_button"
            || element.variant == "kaoss_light_button" || element.variant == "kaoss_red_button") {
            return expandedBounds(element.bounds, 8.0F, 8.0F, 8.0F, 14.0F);
        }
        if (element.variant.rfind("arcade_", 0) == 0) {
            return expandedBounds(element.bounds, 8.0F, 8.0F);
        }
        return expandedBounds(element.bounds, 4.0F, 4.0F);
    case SurfaceElementShape::Knob:
        return expandedBounds(element.bounds, 4.0F, 4.0F);
    case SurfaceElementShape::Fader:
        return expandedBounds(element.bounds, 8.0F, 8.0F);
    case SurfaceElementShape::Joystick:
        return expandedBounds(element.bounds, 8.0F, 8.0F, 8.0F, 30.0F);
    case SurfaceElementShape::Circle:
        return expandedBounds(element.bounds, 8.0F, 8.0F);
    default:
        return expandedBounds(element.bounds, 4.0F, 4.0F);
    }
}

bool surfaceBoundsOverlap(const SurfaceBounds& lhs, const SurfaceBounds& rhs)
{
    return lhs.x < rightEdge(rhs)
        && rightEdge(lhs) > rhs.x
        && lhs.y < bottomEdge(rhs)
        && bottomEdge(lhs) > rhs.y;
}

} // namespace loop_rigger::profile_io
