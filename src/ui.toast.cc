#include "ui.toast.hpp"

#include <algorithm>

#include <d2d1helper.h>

#include "ui.text.hpp"
#include "ui.theme.hpp"

namespace {

constexpr float kToastTop = ui_theme::metrics::kTitleBarHeight + 5.0f;
constexpr float kToastViewportMargin = 8.0f;
constexpr float kToastMaxWidth = 210.0f;
constexpr float kToastMinWidth = 60.0f;
constexpr float kToastPaddingX = 8.0f;
constexpr float kToastPaddingY = 3.5f;
constexpr float kToastCornerRadius = 4.0f;
constexpr float kToastBackgroundOpacity = 0.92f;

D2D1_COLOR_F ToastBackgroundColor()
{
    return D2D1::ColorF(
        ui_theme::color::kToolbarBackground.r,
        ui_theme::color::kToolbarBackground.g,
        ui_theme::color::kToolbarBackground.b,
        kToastBackgroundOpacity);
}

} // namespace

void UiToast::Render(const UiDrawContext& draw_context) const
{
    const D2D1_SIZE_F viewport_size = draw_context.viewport_size;
    if (text_.empty() || viewport_size.width <= kToastViewportMargin * 2.0f) {
        return;
    }

    const UINT32 text_length = static_cast<UINT32>(text_.size());
    const float max_width = (std::min)(kToastMaxWidth, viewport_size.width - kToastViewportMargin * 2.0f);
    const float max_text_width = (std::max)(1.0f, max_width - kToastPaddingX * 2.0f);
    const std::wstring display_text =
        ui_text::TruncateText(
            draw_context.dwrite_factory,
            draw_context.body_text_format,
            text_.c_str(),
            text_length,
            max_text_width);
    if (display_text.empty()) {
        return;
    }

    const ui_text::TextMetrics text_metrics = ui_text::GetTextMetrics(
        draw_context.dwrite_factory,
        draw_context.body_text_format,
        display_text.c_str(),
        static_cast<UINT32>(display_text.size()));
    const float width = std::clamp(text_metrics.width + kToastPaddingX * 2.0f, kToastMinWidth, max_width);
    const float height = text_metrics.height + kToastPaddingY * 2.0f;
    const float left = (viewport_size.width - width) * 0.5f;
    const D2D1_RECT_F rect = D2D1::RectF(left, kToastTop, left + width, kToastTop + height);
    const D2D1_RECT_F text_rect = D2D1::RectF(
        rect.left + kToastPaddingX,
        rect.top + kToastPaddingY,
        rect.right - kToastPaddingX,
        rect.bottom - kToastPaddingY);

    const UiDraw draw(draw_context);
    const D2D1_ROUNDED_RECT rounded_rect = D2D1::RoundedRect(rect, kToastCornerRadius, kToastCornerRadius);
    draw.FillRoundedRect(rounded_rect, ToastBackgroundColor());
    draw.DrawRoundedRect(rounded_rect, ui_theme::color::kBorder, ui_theme::metrics::kStrokeWidth);
    draw.DrawBodyText(
        display_text,
        text_rect,
        ui_theme::color::kBodyText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE_NATURAL);
}

void UiToast::Show(const wchar_t* text)
{
    if (text == nullptr || text[0] == L'\0') {
        Hide();
        return;
    }

    text_ = text;
}

bool UiToast::Hide()
{
    if (text_.empty()) {
        return false;
    }

    text_.clear();
    return true;
}

bool UiToast::IsVisible() const
{
    return !text_.empty();
}
