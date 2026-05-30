#include "ui.button.hpp"

#include <cwchar>

#include <d2d1helper.h>
#include <wil/com.h>

namespace {

D2D1_COLOR_F ButtonFillColor(UiElementState state)
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

Button::Button(UiElementMetadata metadata, const wchar_t* icon, const wchar_t* text) :
    UiElement(metadata),
    icon_(icon),
    text_(text)
{
}

void Button::Draw(
    ID2D1DeviceContext* d2d_context,
    IDWriteTextFormat* body_text_format,
    IDWriteTextFormat* icon_text_format,
    UiElementState state) const
{
    const D2D1_RECT_F rect = Rect();
    wil::com_ptr<ID2D1SolidColorBrush> fill_brush;
    d2d_context->CreateSolidColorBrush(ButtonFillColor(state), fill_brush.put());

    wil::com_ptr<ID2D1SolidColorBrush> stroke_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0xb8c7dc), stroke_brush.put());

    wil::com_ptr<ID2D1SolidColorBrush> text_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0x172033), text_brush.put());

    wil::com_ptr<ID2D1SolidColorBrush> icon_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0x2f6fed), icon_brush.put());

    const D2D1_ROUNDED_RECT button = D2D1::RoundedRect(rect, 6.0f, 6.0f);
    d2d_context->FillRoundedRectangle(button, fill_brush.get());
    d2d_context->DrawRoundedRectangle(button, stroke_brush.get(), 1.0f);
    d2d_context->DrawTextW(
        icon_,
        static_cast<UINT32>(wcslen(icon_)),
        icon_text_format,
        D2D1::RectF(rect.left + 14.0f, rect.top + 10.0f, rect.left + 38.0f, rect.bottom),
        icon_brush.get(),
        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE_NATURAL);
    d2d_context->DrawTextW(
        text_,
        static_cast<UINT32>(wcslen(text_)),
        body_text_format,
        D2D1::RectF(rect.left + 44.0f, rect.top + 5.0f, rect.right - 12.0f, rect.bottom),
        text_brush.get(),
        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE_NATURAL);
}

IconButton::IconButton(UiElementMetadata metadata, const wchar_t* icon) : UiElement(metadata), icon_(icon) {}

void IconButton::SetIcon(const wchar_t* icon)
{
    icon_ = icon;
}

void IconButton::Draw(
    ID2D1DeviceContext* d2d_context,
    IDWriteTextFormat* icon_text_format,
    UiElementState state) const
{
    const D2D1_RECT_F rect = Rect();
    wil::com_ptr<ID2D1SolidColorBrush> fill_brush;
    d2d_context->CreateSolidColorBrush(ButtonFillColor(state), fill_brush.put());

    wil::com_ptr<ID2D1SolidColorBrush> text_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0x172033), text_brush.put());

    wil::com_ptr<ID2D1SolidColorBrush> icon_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0x2f6fed), icon_brush.put());

    d2d_context->FillRectangle(rect, fill_brush.get());
    d2d_context->DrawTextW(
        icon_,
        static_cast<UINT32>(wcslen(icon_)),
        icon_text_format,
        D2D1::RectF(rect.left + 12.0f, rect.top + 10.0f, rect.right, rect.bottom),
        state.danger && state.hovered ? text_brush.get() : icon_brush.get(),
        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE_NATURAL);
}
