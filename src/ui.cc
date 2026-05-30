#include "ui.hpp"

#include <algorithm>
#include <cwchar>

#include <d2d1helper.h>
#include <wil/com.h>

namespace {

constexpr wchar_t kOpenIcon[] = L"\xE8E5";
constexpr wchar_t kOpenText[] = L"Open Image";
constexpr wchar_t kTestIcon[] = L"\xE8FB";
constexpr wchar_t kTestText[] = L"Test Button";
constexpr wchar_t kTopMostIcon[] = L"\xE718";
constexpr wchar_t kMinimizeIcon[] = L"\xE921";
constexpr wchar_t kMaximizeIcon[] = L"\xE922";
constexpr wchar_t kRestoreIcon[] = L"\xE923";
constexpr wchar_t kCloseIcon[] = L"\xE8BB";

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

    UiCommand command = CommandFor(pressed_button);
    if (hovered_button_ == pressed_button && pressed_button == ButtonId::Test) {
        ++button_clicks_;
    } else if (hovered_button_ != pressed_button) {
        command = UiCommand::None;
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
    D2D1_SIZE_F viewport_size,
    IDWriteTextFormat* body_text_format,
    IDWriteTextFormat* icon_text_format)
{
    const D2D1_SIZE_F size = viewport_size;
    titlebar_rect_ = D2D1::RectF(0.0f, 0.0f, size.width, 48.0f);
    close_button_rect_ = D2D1::RectF((std::max)(0.0f, size.width - 48.0f), 0.0f, size.width, 48.0f);
    maximize_button_rect_ = D2D1::RectF(close_button_rect_.left - 48.0f, 0.0f, close_button_rect_.left, 48.0f);
    minimize_button_rect_ = D2D1::RectF(maximize_button_rect_.left - 48.0f, 0.0f, maximize_button_rect_.left, 48.0f);
    top_most_button_rect_ = D2D1::RectF(minimize_button_rect_.left - 48.0f, 0.0f, minimize_button_rect_.left, 48.0f);
    title_text_rect_ = D2D1::RectF(16.0f, 0.0f, (std::max)(17.0f, top_most_button_rect_.left - 12.0f), 48.0f);

    wil::com_ptr<ID2D1SolidColorBrush> fill_brush;
    d2d_context->CreateSolidColorBrush(ButtonFillColor(false, false), fill_brush.put());

    wil::com_ptr<ID2D1SolidColorBrush> stroke_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0xb8c7dc), stroke_brush.put());

    wil::com_ptr<ID2D1SolidColorBrush> text_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0x172033), text_brush.put());

    wil::com_ptr<ID2D1SolidColorBrush> icon_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0x2f6fed), icon_brush.put());

    wil::com_ptr<ID2D1SolidColorBrush> titlebar_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0xffffff, 0.86f), titlebar_brush.put());
    d2d_context->FillRectangle(titlebar_rect_, titlebar_brush.get());
    d2d_context->DrawTextW(
        title_text_.c_str(),
        static_cast<UINT32>(title_text_.size()),
        body_text_format,
        title_text_rect_,
        text_brush.get(),
        D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE_NATURAL);

    const auto draw_caption_button = [&](D2D1_RECT_F rect, ButtonId id, const wchar_t* icon) {
        const bool hovered = hovered_button_ == id;
        const bool pressed = pressed_button_ == id;
        if (id == ButtonId::Close && (hovered || pressed)) {
            fill_brush->SetColor(pressed ? D2D1::ColorF(0xf2b8b5) : D2D1::ColorF(0xffdad6));
        } else if (id == ButtonId::TopMost && top_most_) {
            fill_brush->SetColor(pressed ? D2D1::ColorF(0xdbe7ff) : D2D1::ColorF(0xebf2ff));
        } else {
            fill_brush->SetColor(ButtonFillColor(hovered, pressed));
        }
        d2d_context->FillRectangle(rect, fill_brush.get());
        d2d_context->DrawTextW(
            icon,
            static_cast<UINT32>(wcslen(icon)),
            icon_text_format,
            D2D1::RectF(rect.left + 12.0f, rect.top + 10.0f, rect.right, rect.bottom),
            id == ButtonId::Close && hovered ? text_brush.get() : icon_brush.get(),
            D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
            DWRITE_MEASURING_MODE_NATURAL);
    };

    draw_caption_button(top_most_button_rect_, ButtonId::TopMost, kTopMostIcon);
    draw_caption_button(minimize_button_rect_, ButtonId::Minimize, kMinimizeIcon);
    draw_caption_button(maximize_button_rect_, ButtonId::MaximizeRestore, maximized_ ? kRestoreIcon : kMaximizeIcon);
    draw_caption_button(close_button_rect_, ButtonId::Close, kCloseIcon);

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
    if (Contains(close_button_rect_, point)) {
        return ButtonId::Close;
    }

    if (Contains(maximize_button_rect_, point)) {
        return ButtonId::MaximizeRestore;
    }

    if (Contains(minimize_button_rect_, point)) {
        return ButtonId::Minimize;
    }

    if (Contains(top_most_button_rect_, point)) {
        return ButtonId::TopMost;
    }

    if (Contains(open_button_rect_, point)) {
        return ButtonId::OpenImage;
    }

    if (Contains(test_button_rect_, point)) {
        return ButtonId::Test;
    }

    return ButtonId::None;
}

UiCommand UiController::CommandFor(ButtonId button) const
{
    switch (button) {
    case ButtonId::OpenImage:
        return UiCommand::OpenImage;
    case ButtonId::TopMost:
        return UiCommand::ToggleTopMost;
    case ButtonId::Minimize:
        return UiCommand::Minimize;
    case ButtonId::MaximizeRestore:
        return UiCommand::ToggleMaximize;
    case ButtonId::Close:
        return UiCommand::Close;
    default:
        return UiCommand::None;
    }
}

D2D1_RECT_F UiController::RectFor(ButtonId button) const
{
    switch (button) {
    case ButtonId::OpenImage:
        return open_button_rect_;
    case ButtonId::Test:
        return test_button_rect_;
    case ButtonId::TopMost:
        return top_most_button_rect_;
    case ButtonId::Minimize:
        return minimize_button_rect_;
    case ButtonId::MaximizeRestore:
        return maximize_button_rect_;
    case ButtonId::Close:
        return close_button_rect_;
    default:
        return D2D1::RectF();
    }
}

D2D1_RECT_F UiController::ElementRect(UiElementId id) const
{
    switch (id) {
    case UiElementId::OpenImage:
        return open_button_rect_;
    case UiElementId::Test:
        return test_button_rect_;
    case UiElementId::TopMost:
        return top_most_button_rect_;
    case UiElementId::Minimize:
        return minimize_button_rect_;
    case UiElementId::MaximizeRestore:
        return maximize_button_rect_;
    case UiElementId::Close:
        return close_button_rect_;
    default:
        return D2D1::RectF();
    }
}

bool UiController::IsPointInCaptionDragArea(D2D1_POINT_2F point) const
{
    return Contains(titlebar_rect_, point) && HitTest(point) == ButtonId::None;
}

void UiController::SetTitleText(const wchar_t* title)
{
    title_text_ = title != nullptr && title[0] != L'\0' ? title : L"ImgViewer";
}

void UiController::SetWindowState(bool top_most, bool maximized)
{
    top_most_ = top_most;
    maximized_ = maximized;
}

void UiController::InvokeTestButton()
{
    ++button_clicks_;
}
