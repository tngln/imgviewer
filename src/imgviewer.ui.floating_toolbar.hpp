#pragma once

#include <cstddef>

#include <d2d1_1.h>

#include "ui.draw.hpp"
#include "ui.element.hpp"
#include "ui.panel.hpp"
#include "ui.root.hpp"

struct ImgViewerFloatingToolbarMetrics final {
    float button_size = 0.0f;
    float button_gap = 0.0f;
    float padding = 0.0f;
    float corner_radius = 0.0f;
};

class ImgViewerFloatingToolbar final {
public:
    ImgViewerFloatingToolbar(
        UiElement& root,
        const wchar_t* name);

    StackPanel* Panel() const;
    D2D1_RECT_F Rect() const;
    ImgViewerFloatingToolbarMetrics Metrics() const;

    void SetScalePercent(int percent);
    float ScaledValue(float value) const;
    D2D1_SIZE_F Measure(size_t button_count, float extra_width = 0.0f, size_t extra_item_count = 0) const;
    void ArrangeHidden();
    void ArrangeAboveAnchor(
        D2D1_RECT_F final_rect,
        D2D1_RECT_F anchor_rect,
        size_t button_count,
        float extra_width,
        size_t extra_item_count,
        float gap_above_anchor);
    void RenderBackground(
        const UiDrawContext& draw_context,
        D2D1_COLOR_F border_color,
        float stroke_width) const;

private:
    int scale_percent_ = 125;
    StackPanel* panel_ = nullptr;
    D2D1_RECT_F rect_ = {};
};
