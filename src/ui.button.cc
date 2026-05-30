#include "ui.button.hpp"

#include <cwchar>

#include <d2d1helper.h>
#include <wil/com.h>

namespace {

bool ContainsRect(D2D1_RECT_F rect, D2D1_POINT_2F point)
{
    return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
}

D2D1_COLOR_F ButtonFillColor(UiButtonState state)
{
    if (state.danger && (state.hovered || state.pressed)) {
        return state.pressed ? D2D1::ColorF(0xf2b8b5) : D2D1::ColorF(0xffdad6);
    }

    if (state.pressed) {
        return D2D1::ColorF(0xdbe7ff);
    }

    if (state.hovered || state.active) {
        return D2D1::ColorF(0xebf2ff);
    }

    return D2D1::ColorF(0xffffff);
}

} // namespace

Button::Button(UiButtonMetadata metadata, const wchar_t* icon, const wchar_t* text) :
    metadata_(metadata),
    icon_(icon),
    text_(text)
{
}

void Button::SetRect(D2D1_RECT_F rect)
{
    rect_ = rect;
}

D2D1_RECT_F Button::Rect() const
{
    return rect_;
}

const UiButtonMetadata& Button::Metadata() const
{
    return metadata_;
}

bool Button::Contains(D2D1_POINT_2F point) const
{
    return ContainsRect(rect_, point);
}

void Button::Draw(
    ID2D1DeviceContext* d2d_context,
    IDWriteTextFormat* body_text_format,
    IDWriteTextFormat* icon_text_format,
    UiButtonState state) const
{
    wil::com_ptr<ID2D1SolidColorBrush> fill_brush;
    d2d_context->CreateSolidColorBrush(ButtonFillColor(state), fill_brush.put());

    wil::com_ptr<ID2D1SolidColorBrush> stroke_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0xb8c7dc), stroke_brush.put());

    wil::com_ptr<ID2D1SolidColorBrush> text_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0x172033), text_brush.put());

    wil::com_ptr<ID2D1SolidColorBrush> icon_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0x2f6fed), icon_brush.put());

    const D2D1_ROUNDED_RECT button = D2D1::RoundedRect(rect_, 6.0f, 6.0f);
    d2d_context->FillRoundedRectangle(button, fill_brush.get());
    d2d_context->DrawRoundedRectangle(button, stroke_brush.get(), 1.0f);
    d2d_context->DrawTextW(
        icon_,
        static_cast<UINT32>(wcslen(icon_)),
        icon_text_format,
        D2D1::RectF(rect_.left + 14.0f, rect_.top + 10.0f, rect_.left + 38.0f, rect_.bottom),
        icon_brush.get(),
        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE_NATURAL);
    d2d_context->DrawTextW(
        text_,
        static_cast<UINT32>(wcslen(text_)),
        body_text_format,
        D2D1::RectF(rect_.left + 44.0f, rect_.top + 5.0f, rect_.right - 12.0f, rect_.bottom),
        text_brush.get(),
        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE_NATURAL);
}

IconButton::IconButton(UiButtonMetadata metadata, const wchar_t* icon) : metadata_(metadata), icon_(icon) {}

void IconButton::SetIcon(const wchar_t* icon)
{
    icon_ = icon;
}

void IconButton::SetRect(D2D1_RECT_F rect)
{
    rect_ = rect;
}

D2D1_RECT_F IconButton::Rect() const
{
    return rect_;
}

const UiButtonMetadata& IconButton::Metadata() const
{
    return metadata_;
}

bool IconButton::Contains(D2D1_POINT_2F point) const
{
    return ContainsRect(rect_, point);
}

void IconButton::Draw(
    ID2D1DeviceContext* d2d_context,
    IDWriteTextFormat* icon_text_format,
    UiButtonState state) const
{
    wil::com_ptr<ID2D1SolidColorBrush> fill_brush;
    d2d_context->CreateSolidColorBrush(ButtonFillColor(state), fill_brush.put());

    wil::com_ptr<ID2D1SolidColorBrush> text_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0x172033), text_brush.put());

    wil::com_ptr<ID2D1SolidColorBrush> icon_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0x2f6fed), icon_brush.put());

    d2d_context->FillRectangle(rect_, fill_brush.get());
    d2d_context->DrawTextW(
        icon_,
        static_cast<UINT32>(wcslen(icon_)),
        icon_text_format,
        D2D1::RectF(rect_.left + 12.0f, rect_.top + 10.0f, rect_.right, rect_.bottom),
        state.danger && state.hovered ? text_brush.get() : icon_brush.get(),
        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE_NATURAL);
}
