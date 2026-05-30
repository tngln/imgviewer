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

constexpr UiButtonMetadata kTopMostMetadata{
    UiElementId::TopMost,
    UiCommand::ToggleTopMost,
    L"Top Most",
    L"top-most",
    2,
};
constexpr UiButtonMetadata kMinimizeMetadata{
    UiElementId::Minimize,
    UiCommand::Minimize,
    L"Minimize",
    L"minimize",
    3,
};
constexpr UiButtonMetadata kMaximizeMetadata{
    UiElementId::MaximizeRestore,
    UiCommand::ToggleMaximize,
    L"Maximize or Restore",
    L"maximize-restore",
    4,
};
constexpr UiButtonMetadata kCloseMetadata{
    UiElementId::Close,
    UiCommand::Close,
    L"Close",
    L"close",
    5,
};
constexpr UiButtonMetadata kOpenMetadata{
    UiElementId::OpenImage,
    UiCommand::OpenImage,
    L"Open Image",
    L"open-image",
    6,
};
constexpr UiButtonMetadata kTestMetadata{
    UiElementId::Test,
    UiCommand::None,
    L"Test Button",
    L"test-button",
    7,
};

bool Contains(D2D1_RECT_F rect, D2D1_POINT_2F point)
{
    return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
}

} // namespace

UiController::UiController() :
    top_most_button_(kTopMostMetadata, kTopMostIcon),
    minimize_button_(kMinimizeMetadata, kMinimizeIcon),
    maximize_button_(kMaximizeMetadata, kMaximizeIcon),
    close_button_(kCloseMetadata, kCloseIcon),
    open_button_(kOpenMetadata, kOpenIcon, kOpenText),
    test_button_(kTestMetadata, kTestIcon, kTestText)
{
    top_most_button_.SetRect(D2D1_RECT_F{720.0f, 0.0f, 768.0f, 48.0f});
    minimize_button_.SetRect(D2D1_RECT_F{768.0f, 0.0f, 816.0f, 48.0f});
    maximize_button_.SetRect(D2D1_RECT_F{816.0f, 0.0f, 864.0f, 48.0f});
    close_button_.SetRect(D2D1_RECT_F{864.0f, 0.0f, 912.0f, 48.0f});
    open_button_.SetRect(D2D1_RECT_F{32.0f, 128.0f, 232.0f, 172.0f});
    test_button_.SetRect(D2D1_RECT_F{244.0f, 128.0f, 400.0f, 172.0f});
}

UiEventResult UiController::OnPointerMove(D2D1_POINT_2F point)
{
    const UiElementId was_hovered = hovered_button_;
    hovered_button_ = HitTest(point);

    return UiEventResult{
        .handled = hovered_button_ != UiElementId::None || pressed_button_ != UiElementId::None,
        .needs_render = was_hovered != hovered_button_,
    };
}

UiEventResult UiController::OnPointerDown(D2D1_POINT_2F point)
{
    const UiElementId button = HitTest(point);
    if (button == UiElementId::None) {
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
    if (pressed_button_ == UiElementId::None) {
        return {};
    }

    const UiElementId pressed_button = pressed_button_;
    hovered_button_ = HitTest(point);
    pressed_button_ = UiElementId::None;

    UiCommand command = CommandFor(pressed_button);
    if (hovered_button_ == pressed_button && pressed_button == UiElementId::Test) {
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
    if (hovered_button_ == UiElementId::None) {
        return {};
    }

    hovered_button_ = UiElementId::None;
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
    close_button_.SetRect(D2D1::RectF((std::max)(0.0f, size.width - 48.0f), 0.0f, size.width, 48.0f));
    maximize_button_.SetRect(D2D1::RectF(close_button_.Rect().left - 48.0f, 0.0f, close_button_.Rect().left, 48.0f));
    minimize_button_.SetRect(D2D1::RectF(maximize_button_.Rect().left - 48.0f, 0.0f, maximize_button_.Rect().left, 48.0f));
    top_most_button_.SetRect(D2D1::RectF(minimize_button_.Rect().left - 48.0f, 0.0f, minimize_button_.Rect().left, 48.0f));
    title_text_rect_ = D2D1::RectF(16.0f, 0.0f, (std::max)(17.0f, top_most_button_.Rect().left - 12.0f), 48.0f);
    maximize_button_.SetIcon(maximized_ ? kRestoreIcon : kMaximizeIcon);

    wil::com_ptr<ID2D1SolidColorBrush> stroke_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0xb8c7dc), stroke_brush.put());

    wil::com_ptr<ID2D1SolidColorBrush> text_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0x172033), text_brush.put());

    wil::com_ptr<ID2D1SolidColorBrush> titlebar_brush;
    d2d_context->CreateSolidColorBrush(D2D1::ColorF(0xffffff, 0.86f), titlebar_brush.put());
    d2d_context->FillRectangle(titlebar_rect_, titlebar_brush.get());
    if (!maximized_) {
        d2d_context->DrawRectangle(
            D2D1::RectF(0.5f, 0.5f, (std::max)(0.5f, size.width - 0.5f), (std::max)(0.5f, size.height - 0.5f)),
            stroke_brush.get(),
            1.0f);
    }
    d2d_context->DrawTextW(
        title_text_.c_str(),
        static_cast<UINT32>(title_text_.size()),
        body_text_format,
        title_text_rect_,
        text_brush.get(),
        D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE_NATURAL);

    top_most_button_.Draw(d2d_context, icon_text_format, ButtonState(UiElementId::TopMost, top_most_));
    minimize_button_.Draw(d2d_context, icon_text_format, ButtonState(UiElementId::Minimize));
    maximize_button_.Draw(d2d_context, icon_text_format, ButtonState(UiElementId::MaximizeRestore));
    close_button_.Draw(d2d_context, icon_text_format, ButtonState(UiElementId::Close, false, true));
    open_button_.Draw(d2d_context, body_text_format, icon_text_format, ButtonState(UiElementId::OpenImage));
    test_button_.Draw(d2d_context, body_text_format, icon_text_format, ButtonState(UiElementId::Test));

    wchar_t click_text[64] = {};
    const int click_text_length = swprintf_s(click_text, L"Button clicks: %u", button_clicks_);
    d2d_context->DrawTextW(
        click_text,
        static_cast<UINT32>(click_text_length),
        body_text_format,
        D2D1::RectF(open_button_.Rect().left, open_button_.Rect().bottom + 12.0f, test_button_.Rect().right + 120.0f, open_button_.Rect().bottom + 52.0f),
        text_brush.get(),
        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE_NATURAL);
}

UiElementId UiController::HitTest(D2D1_POINT_2F point) const
{
    if (close_button_.Contains(point)) {
        return UiElementId::Close;
    }

    if (maximize_button_.Contains(point)) {
        return UiElementId::MaximizeRestore;
    }

    if (minimize_button_.Contains(point)) {
        return UiElementId::Minimize;
    }

    if (top_most_button_.Contains(point)) {
        return UiElementId::TopMost;
    }

    if (open_button_.Contains(point)) {
        return UiElementId::OpenImage;
    }

    if (test_button_.Contains(point)) {
        return UiElementId::Test;
    }

    return UiElementId::None;
}

UiCommand UiController::CommandFor(UiElementId id) const
{
    const UiButtonMetadata* metadata = MetadataForElement(id);
    return metadata != nullptr ? metadata->command : UiCommand::None;
}

UiButtonState UiController::ButtonState(UiElementId id, bool active, bool danger) const
{
    return UiButtonState{
        .hovered = hovered_button_ == id,
        .pressed = pressed_button_ == id,
        .active = active,
        .danger = danger,
    };
}

const UiButtonMetadata* UiController::MetadataForElement(UiElementId id) const
{
    switch (id) {
    case UiElementId::TopMost:
        return &top_most_button_.Metadata();
    case UiElementId::Minimize:
        return &minimize_button_.Metadata();
    case UiElementId::MaximizeRestore:
        return &maximize_button_.Metadata();
    case UiElementId::Close:
        return &close_button_.Metadata();
    case UiElementId::OpenImage:
        return &open_button_.Metadata();
    case UiElementId::Test:
        return &test_button_.Metadata();
    default:
        return nullptr;
    }
}

size_t UiController::ElementCount() const
{
    return 6;
}

const UiButtonMetadata* UiController::ElementMetadataAt(size_t index) const
{
    switch (index) {
    case 0:
        return &top_most_button_.Metadata();
    case 1:
        return &minimize_button_.Metadata();
    case 2:
        return &maximize_button_.Metadata();
    case 3:
        return &close_button_.Metadata();
    case 4:
        return &open_button_.Metadata();
    case 5:
        return &test_button_.Metadata();
    default:
        return nullptr;
    }
}

const UiButtonMetadata* UiController::ElementMetadata(UiElementId id) const
{
    return MetadataForElement(id);
}

D2D1_RECT_F UiController::ElementRect(UiElementId id) const
{
    switch (id) {
    case UiElementId::TopMost:
        return top_most_button_.Rect();
    case UiElementId::Minimize:
        return minimize_button_.Rect();
    case UiElementId::MaximizeRestore:
        return maximize_button_.Rect();
    case UiElementId::Close:
        return close_button_.Rect();
    case UiElementId::OpenImage:
        return open_button_.Rect();
    case UiElementId::Test:
        return test_button_.Rect();
    default:
        return D2D1::RectF();
    }
}

bool UiController::IsPointInCaptionDragArea(D2D1_POINT_2F point) const
{
    return Contains(titlebar_rect_, point) && HitTest(point) == UiElementId::None;
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
