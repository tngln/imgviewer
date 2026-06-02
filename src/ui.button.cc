#include "ui.button.hpp"

#include <cmath>
#include <cwchar>

#include <d2d1helper.h>
#include <wil/com.h>

#include "ui.text.hpp"
#include "ui.theme.hpp"

namespace {

D2D1_COLOR_F ButtonFillColor(UiElementState state)
{
    if (state.danger && (state.hovered || state.pressed)) {
        return state.pressed ? ui_theme::color::kDangerPressed : ui_theme::color::kDangerHovered;
    }

    if (state.pressed) {
        return ui_theme::color::kButtonPressed;
    }

    if (state.hovered || state.active) {
        return ui_theme::color::kButtonHovered;
    }

    return ui_theme::color::kButtonDefault;
}

} // namespace

Button::Button(UiElementMetadata metadata, const wchar_t* icon, const wchar_t* text) :
    UiElement(metadata),
    icon_(icon),
    text_(text)
{
}

float Button::PreferredWidth(IDWriteFactory* factory, IDWriteTextFormat* body_text_format) const
{
    const ui_text::TextMetrics metrics =
        ui_text::MeasureText(factory, body_text_format, text_, static_cast<UINT32>(wcslen(text_)));
    return ui_theme::offset::kButtonTextLeft +
        std::ceil(metrics.width) +
        ui_theme::offset::kButtonTextRight;
}

void Button::Draw(const UiDrawContext& context, UiElementState state) const
{
    const D2D1_RECT_F rect = Rect();
    const UiDraw draw(context);
    const D2D1_ROUNDED_RECT button =
        D2D1::RoundedRect(rect, ui_theme::metrics::kButtonCornerRadius, ui_theme::metrics::kButtonCornerRadius);
    draw.FillRoundedRect(button, ButtonFillColor(state));
    draw.DrawRoundedRect(button, ui_theme::color::kBorder, 1.0f);
    draw.DrawIconText(
        icon_,
        static_cast<UINT32>(wcslen(icon_)),
        D2D1::RectF(
            rect.left + ui_theme::offset::kButtonIconLeft,
            rect.top + ui_theme::offset::kButtonIconTop,
            rect.left + ui_theme::offset::kButtonIconRight,
            rect.bottom),
        ui_theme::color::kAccent);
    draw.DrawBodyText(
        text_,
        static_cast<UINT32>(wcslen(text_)),
        D2D1::RectF(
            rect.left + ui_theme::offset::kButtonTextLeft,
            rect.top + ui_theme::offset::kButtonTextTop,
            rect.right - ui_theme::offset::kButtonTextRight,
            rect.bottom),
        ui_theme::color::kBodyText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
}

IconButton::IconButton(UiElementMetadata metadata, const wchar_t* icon) : UiElement(metadata), icon_(icon) {}

IconButton::IconButton(
    UiElementMetadata metadata,
    const icons::PathCommand* icon_path,
    size_t icon_path_count,
    float icon_viewport) :
    UiElement(metadata),
    icon_path_(icon_path),
    icon_path_count_(icon_path_count),
    icon_viewport_(icon_viewport)
{
}

void IconButton::SetIcon(const wchar_t* icon)
{
    icon_ = icon;
    icon_path_ = nullptr;
    icon_path_count_ = 0;
    icon_viewport_ = 0.0f;
}

void IconButton::Draw(const UiDrawContext& context, UiElementState state) const
{
    const D2D1_RECT_F rect = Rect();
    const UiDraw draw(context);
    draw.FillRect(rect, ButtonFillColor(state));
    const D2D1_COLOR_F icon_color =
        state.danger && state.hovered ? ui_theme::color::kBodyText : ui_theme::color::kAccent;
    if (icon_path_ != nullptr && icon_path_count_ > 0 && icon_viewport_ > 0.0f && context.d2d_context != nullptr) {
        wil::com_ptr<ID2D1Factory> factory;
        context.d2d_context->GetFactory(factory.put());
        wil::com_ptr<ID2D1PathGeometry> geometry;
        if (SUCCEEDED(CreatePathGeometryFromIcon(factory.get(), icon_path_, icon_path_count_, geometry.put()))) {
            const float icon_size = 20.0f;
            const float scale = icon_size / icon_viewport_;
            const float left = rect.left + (std::max)(0.0f, (rect.right - rect.left - icon_size) * 0.5f);
            const float top = rect.top + (std::max)(0.0f, (rect.bottom - rect.top - icon_size) * 0.5f);
            D2D1_MATRIX_3X2_F old_transform = {};
            context.d2d_context->GetTransform(&old_transform);
            context.d2d_context->SetTransform(
                D2D1::Matrix3x2F::Scale(scale, scale) *
                D2D1::Matrix3x2F::Translation(left, top) *
                old_transform);
            draw.DrawGeometry(geometry.get(), icon_color, ui_theme::metrics::kPathIconStrokeWidth / scale);
            context.d2d_context->SetTransform(old_transform);
        }
        return;
    }

    draw.DrawIconText(
        icon_,
        static_cast<UINT32>(wcslen(icon_)),
        D2D1::RectF(
            rect.left + ui_theme::offset::kCaptionIconLeft,
            rect.top + ui_theme::offset::kCaptionIconTop,
            rect.right,
            rect.bottom),
        icon_color);
}
