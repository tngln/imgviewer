#include "imgviewer.ui.floating_toolbar.hpp"

#include <algorithm>
#include <memory>

#include <d2d1helper.h>

#include "imgviewer.config.hpp"
#include "ui.layout.hpp"
#include "ui.theme.hpp"

ImgViewerFloatingToolbar::ImgViewerFloatingToolbar(
    UiElement& root,
    const wchar_t* name)
{
    panel_ = static_cast<StackPanel*>(root.AddChild(std::make_unique<StackPanel>(
        UiMetadata(UiElementRole::Pane, kUiActionNone, name, false, false),
        ui_layout::StackDirection::Horizontal)));
    panel_->SetGap(ui_theme::metrics::kToolbarButtonGap);
}

StackPanel* ImgViewerFloatingToolbar::Panel() const
{
    return panel_;
}

D2D1_RECT_F ImgViewerFloatingToolbar::Rect() const
{
    return rect_;
}

ImgViewerFloatingToolbarMetrics ImgViewerFloatingToolbar::Metrics() const
{
    const float scale = static_cast<float>(ClampToolbarScalePercent(scale_percent_)) / 100.0f;
    return ImgViewerFloatingToolbarMetrics{
        .button_size = ui_theme::metrics::kToolbarButtonSize * scale,
        .button_gap = ui_theme::metrics::kToolbarButtonGap * scale,
        .padding = ui_theme::metrics::kToolbarPadding * scale,
        .corner_radius = ui_theme::metrics::kToolbarCornerRadius * scale,
    };
}

void ImgViewerFloatingToolbar::SetScalePercent(int percent)
{
    scale_percent_ = ClampToolbarScalePercent(percent);
}

float ImgViewerFloatingToolbar::ScaledValue(float value) const
{
    return value * static_cast<float>(ClampToolbarScalePercent(scale_percent_)) / 100.0f;
}

D2D1_SIZE_F ImgViewerFloatingToolbar::Measure(size_t button_count, float extra_width, size_t extra_item_count) const
{
    if (button_count == 0 && extra_item_count == 0) {
        return D2D1::SizeF();
    }

    const ImgViewerFloatingToolbarMetrics metrics = Metrics();
    const size_t item_count = button_count + extra_item_count;
    const float gap_width = item_count > 0 ? metrics.button_gap * static_cast<float>(item_count - 1) : 0.0f;
    return D2D1::SizeF(
        metrics.button_size * static_cast<float>(button_count) + extra_width + gap_width + metrics.padding * 2.0f,
        metrics.button_size + metrics.padding * 2.0f);
}

void ImgViewerFloatingToolbar::ArrangeHidden()
{
    rect_ = D2D1::RectF();
    panel_->SetHitTestVisible(false);
    panel_->Arrange(rect_);
}

void ImgViewerFloatingToolbar::ArrangeAboveAnchor(
    D2D1_RECT_F final_rect,
    D2D1_RECT_F anchor_rect,
    size_t button_count,
    float extra_width,
    size_t extra_item_count,
    float gap_above_anchor)
{
    const ImgViewerFloatingToolbarMetrics metrics = Metrics();
    const D2D1_SIZE_F toolbar_size = Measure(button_count, extra_width, extra_item_count);
    const float viewport_width = final_rect.right - final_rect.left;
    const float left = (std::max)(0.0f, (viewport_width - toolbar_size.width) * 0.5f);
    const float preferred_top = anchor_rect.top - toolbar_size.height - gap_above_anchor;
    const float top = (std::max)(ui_theme::metrics::kTitleBarHeight + metrics.padding, preferred_top);
    rect_ = D2D1::RectF(left, top, left + toolbar_size.width, top + toolbar_size.height);

    panel_->SetHitTestVisible(true);
    panel_->SetGap(metrics.button_gap);
    panel_->SetPadding(UiThickness{metrics.padding, metrics.padding, metrics.padding, metrics.padding});
    for (size_t index = 0; index < button_count; ++index) {
        panel_->SetItemFixedMainSize(index, metrics.button_size);
    }
    if (extra_item_count > 0) {
        panel_->SetItemFixedMainSize(button_count, extra_width);
    }
    panel_->Arrange(rect_);
}

void ImgViewerFloatingToolbar::RenderBackground(
    const UiDrawContext& draw_context,
    D2D1_COLOR_F border_color,
    float stroke_width) const
{
    const UiDraw draw(draw_context);
    const ImgViewerFloatingToolbarMetrics metrics = Metrics();
    const D2D1_ROUNDED_RECT background = D2D1::RoundedRect(rect_, metrics.corner_radius, metrics.corner_radius);
    draw.FillRoundedRect(
        background,
        D2D1::ColorF(
            ui_theme::color::kToolbarBackground.r,
            ui_theme::color::kToolbarBackground.g,
            ui_theme::color::kToolbarBackground.b,
            ui_theme::color::kToolbarBackgroundOpacity));
    draw.DrawRoundedRect(background, border_color, stroke_width);
}
