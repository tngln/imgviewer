#pragma once

#include <cstddef>

#include <d2d1_1.h>

namespace icons {

enum class PathVerb {
    MoveTo,
    LineTo,
    CubicTo,
    Close,
};

struct PathCommand {
    PathVerb verb;
    D2D1_POINT_2F points[3];
};

} // namespace icons
