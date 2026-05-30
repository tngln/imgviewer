#pragma once

#include <cstddef>
#include <vector>

#include <d2d1_1.h>

namespace ui_layout {

std::vector<D2D1_RECT_F> PlaceRightAlignedRow(
    D2D1_RECT_F container,
    float item_width,
    float item_height,
    size_t count,
    float gap = 0.0f);
std::vector<D2D1_RECT_F> PlaceHorizontalRow(
    D2D1_POINT_2F origin,
    float height,
    const std::vector<float>& widths,
    float gap = 0.0f);
D2D1_RECT_F Below(D2D1_RECT_F anchor, float gap, float width, float height);

} // namespace ui_layout
