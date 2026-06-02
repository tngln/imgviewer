#include "ui.hpp"

#include <algorithm>
#include <cwchar>
#include <memory>
#include <string>
#include <vector>

#include <d2d1helper.h>

#include "math.hpp"
#include "ui.layout.hpp"
#include "ui.text.hpp"
#include "ui.theme.hpp"

namespace {

constexpr wchar_t kPreviousIcon[] = L"\xE892";
constexpr wchar_t kNextIcon[] = L"\xE893";
constexpr wchar_t kZoomInIcon[] = L"\xE8A3";
constexpr wchar_t kZoomOutIcon[] = L"\xE71F";
constexpr wchar_t kRotateIcon[] = L"\xE7AD";
constexpr wchar_t kResetIcon[] = L"\xE777";
constexpr wchar_t kTopMostIcon[] = L"\xE718";
constexpr wchar_t kMinimizeIcon[] = L"\xE921";
constexpr wchar_t kMaximizeIcon[] = L"\xE922";
constexpr wchar_t kRestoreIcon[] = L"\xE923";
constexpr wchar_t kCloseIcon[] = L"\xE8BB";

constexpr UiElementMetadata kRootMetadata{
    .id = UiElementId::None,
    .role = UiElementRole::Pane,
    .action = AppAction::None,
    .name = L"ImgViewer",
    .tooltip = L"",
    .automation_id = L"root",
    .runtime_id = 1,
    .is_control = true,
    .is_content = true,
};
constexpr UiElementMetadata kTopMostMetadata{
    .id = UiElementId::TopMost,
    .role = UiElementRole::Button,
    .action = AppAction::ToggleTopMost,
    .name = L"Top Most",
    .tooltip = L"Keep window on top",
    .automation_id = L"top-most",
    .runtime_id = 2,
};
constexpr UiElementMetadata kMinimizeMetadata{
    .id = UiElementId::Minimize,
    .role = UiElementRole::Button,
    .action = AppAction::Minimize,
    .name = L"Minimize",
    .tooltip = L"Minimize",
    .automation_id = L"minimize",
    .runtime_id = 3,
};
constexpr UiElementMetadata kMaximizeMetadata{
    .id = UiElementId::MaximizeRestore,
    .role = UiElementRole::Button,
    .action = AppAction::ToggleMaximize,
    .name = L"Maximize or Restore",
    .tooltip = L"Maximize or restore",
    .automation_id = L"maximize-restore",
    .runtime_id = 4,
};
constexpr UiElementMetadata kCloseMetadata{
    .id = UiElementId::Close,
    .role = UiElementRole::Button,
    .action = AppAction::Close,
    .name = L"Close",
    .tooltip = L"Close",
    .automation_id = L"close",
    .runtime_id = 5,
};
constexpr UiElementMetadata kPreviousMetadata{
    .id = UiElementId::PreviousImage,
    .role = UiElementRole::Button,
    .action = AppAction::PreviousImage,
    .name = L"Previous Image",
    .tooltip = L"Previous image",
    .automation_id = L"previous-image",
    .runtime_id = 7,
};
constexpr UiElementMetadata kNextMetadata{
    .id = UiElementId::NextImage,
    .role = UiElementRole::Button,
    .action = AppAction::NextImage,
    .name = L"Next Image",
    .tooltip = L"Next image",
    .automation_id = L"next-image",
    .runtime_id = 8,
};
constexpr UiElementMetadata kZoomInMetadata{
    .id = UiElementId::ZoomIn,
    .role = UiElementRole::Button,
    .action = AppAction::ZoomIn,
    .name = L"Zoom In",
    .tooltip = L"Zoom in",
    .automation_id = L"zoom-in",
    .runtime_id = 9,
};
constexpr UiElementMetadata kZoomOutMetadata{
    .id = UiElementId::ZoomOut,
    .role = UiElementRole::Button,
    .action = AppAction::ZoomOut,
    .name = L"Zoom Out",
    .tooltip = L"Zoom out",
    .automation_id = L"zoom-out",
    .runtime_id = 10,
};
constexpr UiElementMetadata kRotateMetadata{
    .id = UiElementId::RotateClockwise,
    .role = UiElementRole::Button,
    .action = AppAction::RotateClockwise,
    .name = L"Rotate Clockwise",
    .tooltip = L"Rotate clockwise",
    .automation_id = L"rotate-clockwise",
    .runtime_id = 11,
};
constexpr UiElementMetadata kResetMetadata{
    .id = UiElementId::ResetView,
    .role = UiElementRole::Button,
    .action = AppAction::ResetView,
    .name = L"Reset View",
    .tooltip = L"Reset view",
    .automation_id = L"reset-view",
    .runtime_id = 12,
};
constexpr UiElementMetadata kFlipHorizontalMetadata{
    .id = UiElementId::FlipHorizontal,
    .role = UiElementRole::Button,
    .action = AppAction::FlipHorizontal,
    .name = L"Flip Horizontal",
    .tooltip = L"Flip horizontal",
    .automation_id = L"flip-horizontal",
    .runtime_id = 13,
};
constexpr UiElementMetadata kFlipVerticalMetadata{
    .id = UiElementId::FlipVertical,
    .role = UiElementRole::Button,
    .action = AppAction::FlipVertical,
    .name = L"Flip Vertical",
    .tooltip = L"Flip vertical",
    .automation_id = L"flip-vertical",
    .runtime_id = 14,
};
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
    previous_button_ =
        static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(kPreviousMetadata, kPreviousIcon)));
    next_button_ = static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(kNextMetadata, kNextIcon)));
    zoom_in_button_ =
        static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(kZoomInMetadata, kZoomInIcon)));
    zoom_out_button_ =
        static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(kZoomOutMetadata, kZoomOutIcon)));
    rotate_button_ =
        static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(kRotateMetadata, kRotateIcon)));
    flip_horizontal_button_ = static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(
        kFlipHorizontalMetadata,
        icons::kFlipHorizontalIconPath,
        ARRAYSIZE(icons::kFlipHorizontalIconPath),
        icons::kToolbarIconViewport)));
    flip_vertical_button_ = static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(
        kFlipVerticalMetadata,
        icons::kFlipVerticalIconPath,
        ARRAYSIZE(icons::kFlipVerticalIconPath),
        icons::kToolbarIconViewport)));
    reset_button_ =
        static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(kResetMetadata, kResetIcon)));
    SetActionEnabled(AppAction::PreviousImage, false);
    SetActionEnabled(AppAction::NextImage, false);
}

UiEventResult UiController::OnPointerMove(D2D1_POINT_2F point)
{
    if (toolbar_dragging_) {
        toolbar_position_ = D2D1::Point2F(point.x - toolbar_drag_offset_.x, point.y - toolbar_drag_offset_.y);
        toolbar_rect_ = D2D1::RectF(
            toolbar_position_.x,
            toolbar_position_.y,
            toolbar_position_.x + math::RectWidth(toolbar_rect_),
            toolbar_position_.y + math::RectHeight(toolbar_rect_));
        return UiEventResult{
            .handled = true,
            .needs_render = true,
        };
    }

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
    if (button == UiElementId::None && !HitTestToolbar(point)) {
        return {};
    }

    if (button == UiElementId::None) {
        toolbar_dragging_ = true;
        toolbar_drag_offset_ = D2D1::Point2F(point.x - toolbar_position_.x, point.y - toolbar_position_.y);
        hovered_button_ = UiElementId::None;
        pressed_button_ = UiElementId::None;
        return UiEventResult{
            .handled = true,
            .captured = true,
        };
    }

    if (!IsElementEnabled(button)) {
        hovered_button_ = button;
        pressed_button_ = UiElementId::None;
        return UiEventResult{
            .handled = true,
        };
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
    if (toolbar_dragging_) {
        toolbar_dragging_ = false;
        return UiEventResult{
            .handled = true,
            .needs_render = true,
            .released_capture = true,
        };
    }

    if (pressed_button_ == UiElementId::None) {
        return {};
    }

    const UiElementId pressed_button = pressed_button_;
    hovered_button_ = HitTest(point);
    pressed_button_ = UiElementId::None;

    AppAction action = hovered_button_ == pressed_button ? ActionFor(pressed_button) : AppAction::None;

    return UiEventResult{
        .handled = true,
        .needs_render = true,
        .released_capture = true,
        .action = action,
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
    IDWriteFactory* dwrite_factory,
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
    Layout(size, dwrite_factory, body_text_format);
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
    const std::wstring title_text = ui_text::TruncateText(
        dwrite_factory,
        body_text_format,
        title_text_.c_str(),
        static_cast<UINT32>(title_text_.size()),
        math::RectWidth(title_text_rect_));
    draw.DrawBodyText(
        title_text.c_str(),
        static_cast<UINT32>(title_text.size()),
        title_text_rect_,
        ui_theme::color::kBodyText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE_NATURAL);

    top_most_button_->Draw(draw_context, ButtonState(UiElementId::TopMost, top_most_));
    minimize_button_->Draw(draw_context, ButtonState(UiElementId::Minimize));
    maximize_button_->Draw(draw_context, ButtonState(UiElementId::MaximizeRestore));
    close_button_->Draw(draw_context, ButtonState(UiElementId::Close, false, true));

    const D2D1_ROUNDED_RECT toolbar_background = D2D1::RoundedRect(
        toolbar_rect_,
        ui_theme::metrics::kToolbarCornerRadius,
        ui_theme::metrics::kToolbarCornerRadius);
    draw.FillRoundedRect(
        toolbar_background,
        D2D1::ColorF(
            ui_theme::color::kToolbarBackground.r,
            ui_theme::color::kToolbarBackground.g,
            ui_theme::color::kToolbarBackground.b,
            ui_theme::color::kToolbarBackgroundOpacity));
    draw.DrawRoundedRect(toolbar_background, ui_theme::color::kBorder, 1.0f);
    previous_button_->Draw(draw_context, ButtonState(UiElementId::PreviousImage));
    next_button_->Draw(draw_context, ButtonState(UiElementId::NextImage));
    zoom_in_button_->Draw(draw_context, ButtonState(UiElementId::ZoomIn));
    zoom_out_button_->Draw(draw_context, ButtonState(UiElementId::ZoomOut));
    rotate_button_->Draw(draw_context, ButtonState(UiElementId::RotateClockwise));
    flip_horizontal_button_->Draw(draw_context, ButtonState(UiElementId::FlipHorizontal));
    flip_vertical_button_->Draw(draw_context, ButtonState(UiElementId::FlipVertical));
    reset_button_->Draw(draw_context, ButtonState(UiElementId::ResetView));
}

void UiController::Layout(D2D1_SIZE_F viewport_size, IDWriteFactory*, IDWriteTextFormat*)
{
    const D2D1_RECT_F root_rect = D2D1::RectF(0.0f, 0.0f, viewport_size.width, viewport_size.height);
    root_->SetRect(root_rect);

    titlebar_rect_ = D2D1::RectF(0.0f, 0.0f, viewport_size.width, ui_theme::metrics::kTitleBarHeight);
    const float caption_edge_padding = ui_theme::metrics::kCaptionButtonEdgePadding;
    const D2D1_RECT_F caption_button_area = D2D1::RectF(
        titlebar_rect_.left,
        titlebar_rect_.top + caption_edge_padding,
        titlebar_rect_.right - caption_edge_padding,
        titlebar_rect_.bottom);
    const std::vector<D2D1_RECT_F> caption_buttons = ui_layout::PlaceRightAlignedRow(
        caption_button_area,
        ui_theme::metrics::kCaptionButtonWidth,
        ui_theme::metrics::kTitleBarHeight - caption_edge_padding,
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

    constexpr size_t toolbar_button_count = 8;
    const float toolbar_content_width =
        ui_theme::metrics::kToolbarButtonSize * static_cast<float>(toolbar_button_count) +
        ui_theme::metrics::kToolbarButtonGap * static_cast<float>(toolbar_button_count - 1);
    const float toolbar_width = toolbar_content_width +
        ui_theme::metrics::kToolbarPadding * 2.0f +
        ui_theme::metrics::kToolbarDragHandleWidth;
    const float toolbar_height = ui_theme::metrics::kToolbarButtonSize + ui_theme::metrics::kToolbarPadding * 2.0f;
    if (!toolbar_position_initialized_) {
        toolbar_position_ = D2D1::Point2F(
            (std::max)(0.0f, (viewport_size.width - toolbar_width) * 0.5f),
            (std::max)(0.0f, viewport_size.height - toolbar_height - ui_theme::metrics::kToolbarBottomMargin));
        toolbar_position_initialized_ = true;
    }
    toolbar_rect_ = D2D1::RectF(
        toolbar_position_.x,
        toolbar_position_.y,
        toolbar_position_.x + toolbar_width,
        toolbar_position_.y + toolbar_height);
    ClampToolbarToViewport(viewport_size);
    toolbar_position_ = D2D1::Point2F(toolbar_rect_.left, toolbar_rect_.top);

    const std::vector<D2D1_RECT_F> toolbar_buttons = ui_layout::PlaceHorizontalRow(
        D2D1::Point2F(
            toolbar_rect_.left + ui_theme::metrics::kToolbarPadding + ui_theme::metrics::kToolbarDragHandleWidth,
            toolbar_rect_.top + ui_theme::metrics::kToolbarPadding),
        ui_theme::metrics::kToolbarButtonSize,
        std::vector<float>(
            toolbar_button_count,
            ui_theme::metrics::kToolbarButtonSize),
        ui_theme::metrics::kToolbarButtonGap);
    previous_button_->SetRect(toolbar_buttons[0]);
    next_button_->SetRect(toolbar_buttons[1]);
    zoom_in_button_->SetRect(toolbar_buttons[2]);
    zoom_out_button_->SetRect(toolbar_buttons[3]);
    rotate_button_->SetRect(toolbar_buttons[4]);
    flip_horizontal_button_->SetRect(toolbar_buttons[5]);
    flip_vertical_button_->SetRect(toolbar_buttons[6]);
    reset_button_->SetRect(toolbar_buttons[7]);
}

UiElementId UiController::HitTest(D2D1_POINT_2F point) const
{
    const UiElement* hit_element = root_->HitTest(point);
    return hit_element != nullptr ? hit_element->Id() : UiElementId::None;
}

bool UiController::HitTestToolbar(D2D1_POINT_2F point) const
{
    return math::Contains(toolbar_rect_, point);
}

void UiController::ClampToolbarToViewport(D2D1_SIZE_F viewport_size)
{
    const float toolbar_width = math::RectWidth(toolbar_rect_);
    const float toolbar_height = math::RectHeight(toolbar_rect_);
    const float max_left = (std::max)(0.0f, viewport_size.width - toolbar_width);
    const float max_top = (std::max)(0.0f, viewport_size.height - toolbar_height);
    const float left = std::clamp(toolbar_rect_.left, 0.0f, max_left);
    const float top = std::clamp(toolbar_rect_.top, 0.0f, max_top);
    toolbar_rect_ = D2D1::RectF(left, top, left + toolbar_width, top + toolbar_height);
}

AppAction UiController::ActionFor(UiElementId id) const
{
    const UiElementMetadata* metadata = MetadataForElement(id);
    if (metadata == nullptr || !IsActionEnabled(metadata->action)) {
        return AppAction::None;
    }

    return metadata->action;
}

bool UiController::IsActionEnabled(AppAction action) const
{
    if (action == AppAction::None) {
        return false;
    }

    return std::find(disabled_actions_.begin(), disabled_actions_.end(), action) == disabled_actions_.end();
}

UiElementState UiController::ButtonState(UiElementId id, bool active, bool danger) const
{
    return UiElementState{
        .hovered = hovered_button_ == id,
        .pressed = pressed_button_ == id,
        .active = active,
        .danger = danger,
        .enabled = IsElementEnabled(id),
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

bool UiController::IsElementEnabled(UiElementId id) const
{
    const UiElementMetadata* metadata = MetadataForElement(id);
    return metadata != nullptr && IsActionEnabled(metadata->action);
}

bool UiController::IsPointInCaptionDragArea(D2D1_POINT_2F point) const
{
    return math::Contains(titlebar_rect_, point) && HitTest(point) == UiElementId::None;
}

void UiController::SetTitleText(const wchar_t* title)
{
    title_text_ = title != nullptr && title[0] != L'\0' ? title : L"ImgViewer";
}

void UiController::SetActionEnabled(AppAction action, bool enabled)
{
    if (action == AppAction::None) {
        return;
    }

    const auto existing = std::find(disabled_actions_.begin(), disabled_actions_.end(), action);
    if (enabled && existing != disabled_actions_.end()) {
        disabled_actions_.erase(existing);
    } else if (!enabled && existing == disabled_actions_.end()) {
        disabled_actions_.push_back(action);
    }
}

void UiController::SetWindowState(bool top_most, bool maximized)
{
    top_most_ = top_most;
    maximized_ = maximized;
}
