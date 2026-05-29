#include "ui.hpp"

#include <cwchar>

#include <d2d1helper.h>
#include <wil/com.h>

namespace {

constexpr wchar_t kOpenIcon[] = L"\xE8E5";
constexpr wchar_t kOpenText[] = L"Open Image";
constexpr wchar_t kTestIcon[] = L"\xE8FB";
constexpr wchar_t kTestText[] = L"Test Button";

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
    const ButtonId was_hovered = hovered_button_;
    hovered_button_ = HitTest(point);

    return UiEventResult{
        .handled = hovered_button_ != ButtonId::None || pressed_button_ != ButtonId::None,
        .needs_render = was_hovered != hovered_button_,
    };
}

UiEventResult UiController::OnPointerDown(D2D1_POINT_2F point)
{
    const ButtonId button = HitTest(point);
    if (button == ButtonId::None) {
        return {};
    }

    hovered_button_ = button;
    pressed_button_ = button;
    return UiEventResult{
        .handled = true,
        .needs_render = true,
        .captured = true,
    };
}

UiEventResult UiController::OnPointerUp(D2D1_POINT_2F point)
{
    if (pressed_button_ == ButtonId::None) {
        return {};
    }

    const ButtonId pressed_button = pressed_button_;
    hovered_button_ = HitTest(point);
    pressed_button_ = ButtonId::None;

    UiCommand command = UiCommand::None;
    if (hovered_button_ == pressed_button && pressed_button == ButtonId::Test) {
        ++button_clicks_;
    } else if (hovered_button_ == pressed_button && pressed_button == ButtonId::OpenImage) {
        command = UiCommand::OpenImage;
    }

    return UiEventResult{
        .handled = true,
        .needs_render = true,
        .released_capture = true,
        .command = command,
    };
}

UiEventResult UiController::OnPointerLeave()
{
    if (hovered_button_ == ButtonId::None) {
        return {};
    }

    hovered_button_ = ButtonId::None;
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
    d2d_context->CreateSolidColorBrush(ButtonFillColor(false, false), fill_brush.put());

    wil::com_ptr<ID2D1SolidColorBrush> stroke_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0xb8c7dc), stroke_brush.put());

    wil::com_ptr<ID2D1SolidColorBrush> text_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0x172033), text_brush.put());

    wil::com_ptr<ID2D1SolidColorBrush> icon_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0x2f6fed), icon_brush.put());

    const auto draw_button = [&](D2D1_RECT_F rect, ButtonId id, const wchar_t* icon, const wchar_t* text) {
        fill_brush->SetColor(ButtonFillColor(hovered_button_ == id, pressed_button_ == id));
        const D2D1_ROUNDED_RECT button = D2D1::RoundedRect(rect, 6.0f, 6.0f);
        d2d_context->FillRoundedRectangle(button, fill_brush.get());
        d2d_context->DrawRoundedRectangle(button, stroke_brush.get(), 1.0f);
        d2d_context->DrawTextW(
            icon,
            static_cast<UINT32>(wcslen(icon)),
            icon_text_format,
            D2D1::RectF(rect.left + 14.0f, rect.top + 10.0f, rect.left + 38.0f, rect.bottom),
            icon_brush.get(),
            D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
            DWRITE_MEASURING_MODE_NATURAL);
        d2d_context->DrawTextW(
            text,
            static_cast<UINT32>(wcslen(text)),
            body_text_format,
            D2D1::RectF(rect.left + 44.0f, rect.top + 5.0f, rect.right - 12.0f, rect.bottom),
            text_brush.get(),
            D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
            DWRITE_MEASURING_MODE_NATURAL);
    };

    draw_button(open_button_rect_, ButtonId::OpenImage, kOpenIcon, kOpenText);
    draw_button(test_button_rect_, ButtonId::Test, kTestIcon, kTestText);

    wchar_t click_text[64] = {};
    const int click_text_length = swprintf_s(click_text, L"Button clicks: %u", button_clicks_);
    d2d_context->DrawTextW(
        click_text,
        static_cast<UINT32>(click_text_length),
        body_text_format,
        D2D1::RectF(open_button_rect_.left, open_button_rect_.bottom + 12.0f, test_button_rect_.right + 120.0f, open_button_rect_.bottom + 52.0f),
        text_brush.get(),
        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE_NATURAL);
}

UiController::ButtonId UiController::HitTest(D2D1_POINT_2F point) const
{
    if (Contains(open_button_rect_, point)) {
        return ButtonId::OpenImage;
    }

    if (Contains(test_button_rect_, point)) {
        return ButtonId::Test;
    }

    return ButtonId::None;
}

D2D1_RECT_F UiController::TestButtonRect() const
{
    return test_button_rect_;
}

D2D1_RECT_F UiController::OpenButtonRect() const
{
    return open_button_rect_;
}

void UiController::InvokeTestButton()
{
    ++button_clicks_;
}
