#include "ui.button.hpp"

#include <cwchar>

#include <d2d1helper.h>

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
        ui_theme::color::kBodyText);
}

IconButton::IconButton(UiElementMetadata metadata, const wchar_t* icon) : UiElement(metadata), icon_(icon) {}

void IconButton::SetIcon(const wchar_t* icon)
{
    icon_ = icon;
}

void IconButton::Draw(const UiDrawContext& context, UiElementState state) const
{
    const D2D1_RECT_F rect = Rect();
    const UiDraw draw(context);
    draw.FillRect(rect, ButtonFillColor(state));
    draw.DrawIconText(
        icon_,
        static_cast<UINT32>(wcslen(icon_)),
        D2D1::RectF(
            rect.left + ui_theme::offset::kCaptionIconLeft,
            rect.top + ui_theme::offset::kCaptionIconTop,
            rect.right,
            rect.bottom),
        state.danger && state.hovered ? ui_theme::color::kBodyText : ui_theme::color::kAccent);
}
