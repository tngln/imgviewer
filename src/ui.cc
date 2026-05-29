#include "ui.hpp"

#include <cwchar>

#include <d2d1helper.h>
#include <wil/com.h>

namespace {

constexpr wchar_t kButtonIcon[] = L"\xE8FB";
constexpr wchar_t kButtonText[] = L"Test Button";

bool Contains(D2D1_RECT_F rect, D2D1_POINT_2F point)
{
    return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
}

D2D1_COLOR_F ButtonFillColor(bool hovered, bool pressed)
{
    if (pressed) {
        return D2D1::ColorF(0xdbe7ff);
    }

    if (hovered) {
        return D2D1::ColorF(0xebf2ff);
    }

    return D2D1::ColorF(0xffffff);
}

} // namespace

UiEventResult UiController::OnPointerMove(D2D1_POINT_2F point)
{
    const bool was_hovered = button_hovered_;
    button_hovered_ = HitTestButton(point);

    return UiEventResult{
        .handled = button_hovered_ || button_pressed_,
        .needs_render = was_hovered != button_hovered_,
    };
}

UiEventResult UiController::OnPointerDown(D2D1_POINT_2F point)
{
    if (!HitTestButton(point)) {
        return {};
    }

    button_hovered_ = true;
    button_pressed_ = true;
    return UiEventResult{
        .handled = true,
        .needs_render = true,
        .captured = true,
    };
}

UiEventResult UiController::OnPointerUp(D2D1_POINT_2F point)
{
    if (!button_pressed_) {
        return {};
    }

    button_pressed_ = false;
    button_hovered_ = HitTestButton(point);
    if (button_hovered_) {
        ++button_clicks_;
    }

    return UiEventResult{
        .handled = true,
        .needs_render = true,
        .released_capture = true,
    };
}

UiEventResult UiController::OnPointerLeave()
{
    if (!button_hovered_) {
        return {};
    }

    button_hovered_ = false;
    return UiEventResult{
        .handled = true,
        .needs_render = true,
    };
}

void UiController::Draw(
    ID2D1DeviceContext* d2d_context,
    IDWriteTextFormat* body_text_format,
    IDWriteTextFormat* icon_text_format)
{
    wil::com_ptr<ID2D1SolidColorBrush> fill_brush;
    d2d_context->CreateSolidColorBrush(ButtonFillColor(button_hovered_, button_pressed_), fill_brush.put());

    wil::com_ptr<ID2D1SolidColorBrush> stroke_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0xb8c7dc), stroke_brush.put());

    wil::com_ptr<ID2D1SolidColorBrush> text_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0x172033), text_brush.put());

    wil::com_ptr<ID2D1SolidColorBrush> icon_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0x2f6fed), icon_brush.put());

    const D2D1_ROUNDED_RECT button = D2D1::RoundedRect(button_rect_, 6.0f, 6.0f);
    d2d_context->FillRoundedRectangle(button, fill_brush.get());
    d2d_context->DrawRoundedRectangle(button, stroke_brush.get(), 1.0f);
    d2d_context->DrawTextW(
        kButtonIcon,
        ARRAYSIZE(kButtonIcon) - 1,
        icon_text_format,
        D2D1::RectF(button_rect_.left + 14.0f, button_rect_.top + 10.0f, button_rect_.left + 38.0f, button_rect_.bottom),
        icon_brush.get(),
        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE_NATURAL);
    d2d_context->DrawTextW(
        kButtonText,
        ARRAYSIZE(kButtonText) - 1,
        body_text_format,
        D2D1::RectF(button_rect_.left + 44.0f, button_rect_.top + 5.0f, button_rect_.right - 12.0f, button_rect_.bottom),
        text_brush.get(),
        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE_NATURAL);

    wchar_t click_text[64] = {};
    const int click_text_length = swprintf_s(click_text, L"Button clicks: %u", button_clicks_);
    d2d_context->DrawTextW(
        click_text,
        static_cast<UINT32>(click_text_length),
        body_text_format,
        D2D1::RectF(button_rect_.left, button_rect_.bottom + 12.0f, button_rect_.right + 120.0f, button_rect_.bottom + 52.0f),
        text_brush.get(),
        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE_NATURAL);
}

bool UiController::HitTestButton(D2D1_POINT_2F point) const
{
    return Contains(button_rect_, point);
}

D2D1_RECT_F UiController::TestButtonRect() const
{
    return button_rect_;
}

void UiController::InvokeTestButton()
{
    ++button_clicks_;
}
