#include "ui.hpp"

#include <algorithm>
#include <cwchar>
#include <memory>

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

constexpr UiElementMetadata kRootMetadata{
    .id = UiElementId::None,
    .role = UiElementRole::Pane,
    .command = UiCommand::None,
    .name = L"ImgViewer",
    .automation_id = L"root",
    .runtime_id = 1,
    .is_control = true,
    .is_content = true,
};
constexpr UiElementMetadata kTopMostMetadata{
    .id = UiElementId::TopMost,
    .role = UiElementRole::Button,
    .command = UiCommand::ToggleTopMost,
    .name = L"Top Most",
    .automation_id = L"top-most",
    .runtime_id = 2,
};
constexpr UiElementMetadata kMinimizeMetadata{
    .id = UiElementId::Minimize,
    .role = UiElementRole::Button,
    .command = UiCommand::Minimize,
    .name = L"Minimize",
    .automation_id = L"minimize",
    .runtime_id = 3,
};
constexpr UiElementMetadata kMaximizeMetadata{
    .id = UiElementId::MaximizeRestore,
    .role = UiElementRole::Button,
    .command = UiCommand::ToggleMaximize,
    .name = L"Maximize or Restore",
    .automation_id = L"maximize-restore",
    .runtime_id = 4,
};
constexpr UiElementMetadata kCloseMetadata{
    .id = UiElementId::Close,
    .role = UiElementRole::Button,
    .command = UiCommand::Close,
    .name = L"Close",
    .automation_id = L"close",
    .runtime_id = 5,
};
constexpr UiElementMetadata kOpenMetadata{
    .id = UiElementId::OpenImage,
    .role = UiElementRole::Button,
    .command = UiCommand::OpenImage,
    .name = L"Open Image",
    .automation_id = L"open-image",
    .runtime_id = 6,
};
constexpr UiElementMetadata kTestMetadata{
    .id = UiElementId::Test,
    .role = UiElementRole::Button,
    .command = UiCommand::None,
    .name = L"Test Button",
    .automation_id = L"test-button",
    .runtime_id = 7,
};

bool Contains(D2D1_RECT_F rect, D2D1_POINT_2F point)
{
    return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
}

} // namespace

UiController::UiController() : root_(std::make_unique<UiElement>(kRootMetadata))
{
    top_most_button_ =
        static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(kTopMostMetadata, kTopMostIcon)));
    minimize_button_ =
        static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(kMinimizeMetadata, kMinimizeIcon)));
    maximize_button_ =
        static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(kMaximizeMetadata, kMaximizeIcon)));
    close_button_ = static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(kCloseMetadata, kCloseIcon)));
    open_button_ = static_cast<Button*>(root_->AddChild(std::make_unique<Button>(kOpenMetadata, kOpenIcon, kOpenText)));
    test_button_ = static_cast<Button*>(root_->AddChild(std::make_unique<Button>(kTestMetadata, kTestIcon, kTestText)));

    top_most_button_->SetRect(D2D1_RECT_F{720.0f, 0.0f, 768.0f, 48.0f});
    minimize_button_->SetRect(D2D1_RECT_F{768.0f, 0.0f, 816.0f, 48.0f});
    maximize_button_->SetRect(D2D1_RECT_F{816.0f, 0.0f, 864.0f, 48.0f});
    close_button_->SetRect(D2D1_RECT_F{864.0f, 0.0f, 912.0f, 48.0f});
    open_button_->SetRect(D2D1_RECT_F{32.0f, 128.0f, 232.0f, 172.0f});
    test_button_->SetRect(D2D1_RECT_F{244.0f, 128.0f, 400.0f, 172.0f});
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
    root_->SetRect(D2D1::RectF(0.0f, 0.0f, size.width, size.height));
    titlebar_rect_ = D2D1::RectF(0.0f, 0.0f, size.width, 48.0f);
    close_button_->SetRect(D2D1::RectF((std::max)(0.0f, size.width - 48.0f), 0.0f, size.width, 48.0f));
    maximize_button_->SetRect(D2D1::RectF(close_button_->Rect().left - 48.0f, 0.0f, close_button_->Rect().left, 48.0f));
    minimize_button_->SetRect(D2D1::RectF(maximize_button_->Rect().left - 48.0f, 0.0f, maximize_button_->Rect().left, 48.0f));
    top_most_button_->SetRect(D2D1::RectF(minimize_button_->Rect().left - 48.0f, 0.0f, minimize_button_->Rect().left, 48.0f));
    title_text_rect_ = D2D1::RectF(16.0f, 0.0f, (std::max)(17.0f, top_most_button_->Rect().left - 12.0f), 48.0f);
    maximize_button_->SetIcon(maximized_ ? kRestoreIcon : kMaximizeIcon);

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

    const UiDrawContext draw_context{
        .d2d_context = d2d_context,
        .body_text_format = body_text_format,
        .icon_text_format = icon_text_format,
    };
    top_most_button_->Draw(draw_context, ButtonState(UiElementId::TopMost, top_most_));
    minimize_button_->Draw(draw_context, ButtonState(UiElementId::Minimize));
    maximize_button_->Draw(draw_context, ButtonState(UiElementId::MaximizeRestore));
    close_button_->Draw(draw_context, ButtonState(UiElementId::Close, false, true));
    open_button_->Draw(draw_context, ButtonState(UiElementId::OpenImage));
    test_button_->Draw(draw_context, ButtonState(UiElementId::Test));

    wchar_t click_text[64] = {};
    const int click_text_length = swprintf_s(click_text, L"Button clicks: %u", button_clicks_);
    d2d_context->DrawTextW(
        click_text,
        static_cast<UINT32>(click_text_length),
        body_text_format,
        D2D1::RectF(open_button_->Rect().left, open_button_->Rect().bottom + 12.0f, test_button_->Rect().right + 120.0f, open_button_->Rect().bottom + 52.0f),
        text_brush.get(),
        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE_NATURAL);
}

UiElementId UiController::HitTest(D2D1_POINT_2F point) const
{
    const UiElement* hit_element = root_->HitTest(point);
    return hit_element != nullptr ? hit_element->Id() : UiElementId::None;
}

UiCommand UiController::CommandFor(UiElementId id) const
{
    const UiElementMetadata* metadata = MetadataForElement(id);
    return metadata != nullptr ? metadata->command : UiCommand::None;
}

UiElementState UiController::ButtonState(UiElementId id, bool active, bool danger) const
{
    return UiElementState{
        .hovered = hovered_button_ == id,
        .pressed = pressed_button_ == id,
        .active = active,
        .danger = danger,
    };
}

const UiElementMetadata* UiController::MetadataForElement(UiElementId id) const
{
    const UiElement* element = root_->FindById(id);
    return element != nullptr && element != root_.get() ? &element->Metadata() : nullptr;
}

size_t UiController::ElementCount() const
{
    return root_->ChildCount();
}

const UiElementMetadata* UiController::ElementMetadataAt(size_t index) const
{
    const UiElement* element = root_->ChildAt(index);
    return element != nullptr ? &element->Metadata() : nullptr;
}

const UiElementMetadata* UiController::ElementMetadata(UiElementId id) const
{
    return MetadataForElement(id);
}

D2D1_RECT_F UiController::ElementRect(UiElementId id) const
{
    const UiElement* element = root_->FindById(id);
    return element != nullptr && element != root_.get() ? element->Rect() : D2D1::RectF();
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
