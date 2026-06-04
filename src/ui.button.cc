#include "ui.button.hpp"

#include <cmath>
#include <cwchar>

#include <d2d1helper.h>
#include <wil/com.h>

#include "ui.events.hpp"
#include "ui.text.hpp"
#include "ui.theme.hpp"

namespace {

D2D1_COLOR_F ButtonFillColor(UiElementState state)
{
    if (!state.enabled) {
        return ui_theme::color::kButtonDisabled;
    }

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

UiEventResult ButtonPointerEvent(UiElement& button, const UiPointerEvent& event)
{
    if (event.button != UiPointerButton::Left &&
        (event.type == UiEventType::PointerDown || event.type == UiEventType::PointerUp)) {
        return {};
    }

    if (event.type == UiEventType::PointerDown) {
        const bool can_activate = button.IsEnabled();
        return UiEventResult{
            .handled = true,
            .needs_render = can_activate,
            .capture = can_activate ? UiCaptureRequest::Capture : UiCaptureRequest::None,
            .focus = can_activate && button.IsFocusable() ? UiFocusRequest::FocusTarget : UiFocusRequest::None,
            .focus_target = can_activate ? button.Id() : UiElementId::None,
        };
    }

    if (event.type == UiEventType::PointerUp && event.captured == button.Id()) {
        return UiEventResult{
            .handled = true,
            .needs_render = button.IsEnabled(),
            .capture = UiCaptureRequest::Release,
            .action = button.IsEnabled() && event.target == button.Id() ? button.Action() : ImgViewerAction::None,
        };
    }

    return {};
}

UiEventResult ButtonKeyEvent(UiElement& button, const UiKeyEvent& event)
{
    if (event.type != UiEventType::KeyDown || !button.IsEnabled()) {
        return {};
    }

    if (event.virtual_key != VK_RETURN && event.virtual_key != VK_SPACE) {
        return {};
    }

    return UiEventResult{
        .handled = true,
        .needs_render = true,
        .action = button.Action(),
    };
}

} // namespace

Button::Button(UiElementMetadata metadata, const wchar_t* icon, const wchar_t* text) :
    UiElement(metadata),
    icon_(icon),
    text_(text)
{
    SetFocusable(true);
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
    const D2D1_COLOR_F content_color =
        state.enabled ? ui_theme::color::kAccent : ui_theme::color::kButtonDisabledContent;
    const D2D1_COLOR_F text_color =
        state.enabled ? ui_theme::color::kBodyText : ui_theme::color::kButtonDisabledContent;
    draw.DrawIconText(
        icon_,
        static_cast<UINT32>(wcslen(icon_)),
        D2D1::RectF(
            rect.left + ui_theme::offset::kButtonIconLeft,
            rect.top + ui_theme::offset::kButtonIconTop,
            rect.left + ui_theme::offset::kButtonIconRight,
            rect.bottom),
        content_color);
    draw.DrawBodyText(
        text_,
        static_cast<UINT32>(wcslen(text_)),
        D2D1::RectF(
            rect.left + ui_theme::offset::kButtonTextLeft,
            rect.top + ui_theme::offset::kButtonTextTop,
            rect.right - ui_theme::offset::kButtonTextRight,
            rect.bottom),
        text_color,
        D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
}

UiEventResult Button::OnPointerEvent(const UiPointerEvent& event)
{
    return ButtonPointerEvent(*this, event);
}

UiEventResult Button::OnKeyEvent(const UiKeyEvent& event)
{
    return ButtonKeyEvent(*this, event);
}

IconButton::IconButton(UiElementMetadata metadata, const wchar_t* icon) : UiElement(metadata), icon_(icon)
{
    SetFocusable(true);
}

IconButton::IconButton(
    UiElementMetadata metadata,
    const icons::PathIcon& icon) :
    UiElement(metadata),
    path_icon_(&icon)
{
    SetFocusable(true);
}

void IconButton::SetIcon(const wchar_t* icon)
{
    icon_ = icon;
    path_icon_ = nullptr;
}

void IconButton::Draw(const UiDrawContext& context, UiElementState state) const
{
    const D2D1_RECT_F rect = Rect();
    const UiDraw draw(context);
    draw.FillRect(rect, ButtonFillColor(state));
    const D2D1_COLOR_F icon_color = !state.enabled
        ? ui_theme::color::kButtonDisabledContent
        : state.danger && state.hovered ? ui_theme::color::kBodyText : ui_theme::color::kAccent;
    if (path_icon_ != nullptr && path_icon_->command_count > 0 && context.d2d_context != nullptr) {
        wil::com_ptr<ID2D1Factory> factory;
        context.d2d_context->GetFactory(factory.put());
        wil::com_ptr<ID2D1PathGeometry> geometry;
        if (SUCCEEDED(CreatePathGeometryFromIcon(
                factory.get(),
                path_icon_->commands,
                path_icon_->command_count,
                geometry.put()))) {
            const float icon_size = 20.0f;
            const float icon_width = path_icon_->view_box.right - path_icon_->view_box.left;
            const float icon_height = path_icon_->view_box.bottom - path_icon_->view_box.top;
            const float icon_viewport = (std::max)(icon_width, icon_height);
            const float scale = icon_size / icon_viewport;
            const float left = rect.left + (std::max)(0.0f, (rect.right - rect.left - icon_size) * 0.5f);
            const float top = rect.top + (std::max)(0.0f, (rect.bottom - rect.top - icon_size) * 0.5f);
            D2D1_MATRIX_3X2_F old_transform = {};
            context.d2d_context->GetTransform(&old_transform);
            context.d2d_context->SetTransform(
                D2D1::Matrix3x2F::Translation(-path_icon_->view_box.left, -path_icon_->view_box.top) *
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

UiEventResult IconButton::OnPointerEvent(const UiPointerEvent& event)
{
    return ButtonPointerEvent(*this, event);
}

UiEventResult IconButton::OnKeyEvent(const UiKeyEvent& event)
{
    return ButtonKeyEvent(*this, event);
}
