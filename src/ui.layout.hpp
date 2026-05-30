#pragma once

#include <cstddef>
#include <vector>

#include <d2d1_1.h>

namespace ui_layout {

float RectWidth(D2D1_RECT_F rect);
float RectHeight(D2D1_RECT_F rect);
D2D1_RECT_F Inset(D2D1_RECT_F rect, float all);
D2D1_RECT_F Inset(D2D1_RECT_F rect, float left, float top, float right, float bottom);
D2D1_RECT_F StableRect(float left, float top, float right, float bottom);
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
