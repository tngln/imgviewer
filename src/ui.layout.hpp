#pragma once

#include <cstddef>
#include <vector>

#include <d2d1_1.h>

namespace ui_layout {

enum class StackDirection {
    Horizontal,
    Vertical,
};

enum class StackCrossAxis {
    Start,
    Stretch,
};

struct StackLayout final {
    D2D1_RECT_F bounds = {};
    StackDirection direction = StackDirection::Vertical;
    std::vector<float> item_sizes;
    float gap = 0.0f;
    StackCrossAxis cross_axis = StackCrossAxis::Stretch;
    float cross_axis_size = 0.0f;
};

std::vector<D2D1_RECT_F> PlaceStack(const StackLayout& layout);
std::vector<D2D1_RECT_F> PlaceVerticalStack(
    D2D1_RECT_F bounds,
    const std::vector<float>& heights,
    float gap = 0.0f);
std::vector<D2D1_RECT_F> PlaceHorizontalStack(
    D2D1_RECT_F bounds,
    const std::vector<float>& widths,
    float gap = 0.0f);
std::vector<D2D1_RECT_F> PlaceBottomRightRow(
    D2D1_RECT_F container,
    const std::vector<float>& widths,
    float item_height,
    float right_padding = 0.0f,
    float bottom_padding = 0.0f,
    float gap = 0.0f);
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
