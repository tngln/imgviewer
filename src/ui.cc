#include "ui.hpp"

#include <algorithm>
#include <cwchar>
#include <memory>
#include <vector>

#include <d2d1helper.h>

#include "ui.layout.hpp"
#include "ui.theme.hpp"

namespace {

constexpr wchar_t kOpenIcon[] = L"\xE8E5";
constexpr wchar_t kOpenText[] = L"Open Image";
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

    UiCommand command = hovered_button_ == pressed_button ? CommandFor(pressed_button) : UiCommand::None;

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
    const UiDrawContext draw_context{
        .d2d_context = d2d_context,
        .body_text_format = body_text_format,
        .icon_text_format = icon_text_format,
    };
    const UiDraw draw(draw_context);
    Layout(size);
    maximize_button_->SetIcon(maximized_ ? kRestoreIcon : kMaximizeIcon);
    draw.FillRect(
        titlebar_rect_,
        D2D1::ColorF(ui_theme::color::kTitleBarBackground.r, ui_theme::color::kTitleBarBackground.g,
            ui_theme::color::kTitleBarBackground.b, ui_theme::color::kTitleBarBackgroundOpacity));
    if (!maximized_) {
        draw.DrawRect(
            D2D1::RectF(
                ui_theme::metrics::kWindowBorderInset,
                ui_theme::metrics::kWindowBorderInset,
                (std::max)(ui_theme::metrics::kWindowBorderMinimum, size.width - ui_theme::metrics::kWindowBorderInset),
                (std::max)(ui_theme::metrics::kWindowBorderMinimum, size.height - ui_theme::metrics::kWindowBorderInset)),
            ui_theme::color::kBorder,
            1.0f);
    }
    draw.DrawBodyText(
        title_text_.c_str(),
        static_cast<UINT32>(title_text_.size()),
        title_text_rect_,
        ui_theme::color::kBodyText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE_NATURAL);

    top_most_button_->Draw(draw_context, ButtonState(UiElementId::TopMost, top_most_));
    minimize_button_->Draw(draw_context, ButtonState(UiElementId::Minimize));
    maximize_button_->Draw(draw_context, ButtonState(UiElementId::MaximizeRestore));
    close_button_->Draw(draw_context, ButtonState(UiElementId::Close, false, true));
    open_button_->Draw(draw_context, ButtonState(UiElementId::OpenImage));
}

void UiController::Layout(D2D1_SIZE_F viewport_size)
{
    const D2D1_RECT_F root_rect = D2D1::RectF(0.0f, 0.0f, viewport_size.width, viewport_size.height);
    root_->SetRect(root_rect);

    titlebar_rect_ = D2D1::RectF(0.0f, 0.0f, viewport_size.width, ui_theme::metrics::kTitleBarHeight);
    const std::vector<D2D1_RECT_F> caption_buttons = ui_layout::PlaceRightAlignedRow(
        titlebar_rect_,
        ui_theme::metrics::kCaptionButtonWidth,
        ui_theme::metrics::kTitleBarHeight,
        4);
    close_button_->SetRect(caption_buttons[0]);
    maximize_button_->SetRect(caption_buttons[1]);
    minimize_button_->SetRect(caption_buttons[2]);
    top_most_button_->SetRect(caption_buttons[3]);

    title_text_rect_ = D2D1::RectF(
        ui_theme::metrics::kTitleTextLeft,
        0.0f,
        (std::max)(
            ui_theme::metrics::kTitleTextLeft + 1.0f,
            top_most_button_->Rect().left - ui_theme::metrics::kTitleTextRightPadding),
        ui_theme::metrics::kTitleBarHeight);

    const std::vector<D2D1_RECT_F> primary_buttons = ui_layout::PlaceHorizontalRow(
        D2D1::Point2F(ui_theme::metrics::kPanelPadding, ui_theme::metrics::kPrimaryButtonTop),
        ui_theme::metrics::kPrimaryButtonHeight,
        std::vector<float>{ui_theme::metrics::kOpenButtonWidth});
    open_button_->SetRect(primary_buttons[0]);
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
