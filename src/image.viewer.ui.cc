#include "image.viewer.ui.hpp"

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

UiElementMetadata Metadata(
    UiElementId id,
    UiElementRole role,
    AppAction action,
    const wchar_t* name,
    const wchar_t* tooltip,
    const wchar_t* automation_id,
    bool is_control = true,
    bool is_content = true)
{
    return UiElementMetadata{
        .id = id,
        .role = role,
        .action = action,
        .name = name,
        .tooltip = tooltip,
        .automation_id = automation_id,
        .is_control = is_control,
        .is_content = is_content,
    };
}

} // namespace

ImageViewerUi::ImageViewerUi() :
    root_(std::make_unique<UiElement>(
        Metadata(UiElementId::None, UiElementRole::Pane, AppAction::None, L"ImgViewer", L"", L"root")))
{
    UiElementIdGenerator ids;
    top_most_id_ = ids.Next();
    minimize_id_ = ids.Next();
    maximize_restore_id_ = ids.Next();
    close_id_ = ids.Next();
    previous_image_id_ = ids.Next();
    next_image_id_ = ids.Next();
    zoom_in_id_ = ids.Next();
    zoom_out_id_ = ids.Next();
    rotate_clockwise_id_ = ids.Next();
    reset_view_id_ = ids.Next();
    flip_horizontal_id_ = ids.Next();
    flip_vertical_id_ = ids.Next();
    toolbar_drag_handle_id_ = ids.Next();

    top_most_button_ =
        static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(
            Metadata(top_most_id_, UiElementRole::Button, AppAction::ToggleTopMost, L"Top Most", L"Keep window on top", L"top-most"),
            kTopMostIcon)));
    minimize_button_ =
        static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(
            Metadata(minimize_id_, UiElementRole::Button, AppAction::Minimize, L"Minimize", L"Minimize", L"minimize"),
            kMinimizeIcon)));
    maximize_button_ =
        static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(
            Metadata(maximize_restore_id_, UiElementRole::Button, AppAction::ToggleMaximize, L"Maximize or Restore", L"Maximize or restore", L"maximize-restore"),
            kMaximizeIcon)));
    close_button_ = static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(
        Metadata(close_id_, UiElementRole::Button, AppAction::Close, L"Close", L"Close", L"close"),
        kCloseIcon)));
    previous_button_ =
        static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(
            Metadata(previous_image_id_, UiElementRole::Button, AppAction::PreviousImage, L"Previous Image", L"Previous image", L"previous-image"),
            kPreviousIcon)));
    next_button_ = static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(
        Metadata(next_image_id_, UiElementRole::Button, AppAction::NextImage, L"Next Image", L"Next image", L"next-image"),
        kNextIcon)));
    zoom_in_button_ =
        static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(
            Metadata(zoom_in_id_, UiElementRole::Button, AppAction::ZoomIn, L"Zoom In", L"Zoom in", L"zoom-in"),
            kZoomInIcon)));
    zoom_out_button_ =
        static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(
            Metadata(zoom_out_id_, UiElementRole::Button, AppAction::ZoomOut, L"Zoom Out", L"Zoom out", L"zoom-out"),
            kZoomOutIcon)));
    rotate_button_ =
        static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(
            Metadata(rotate_clockwise_id_, UiElementRole::Button, AppAction::RotateClockwise, L"Rotate Clockwise", L"Rotate clockwise", L"rotate-clockwise"),
            kRotateIcon)));
    flip_horizontal_button_ = static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(
        Metadata(flip_horizontal_id_, UiElementRole::Button, AppAction::FlipHorizontal, L"Flip Horizontal", L"Flip horizontal", L"flip-horizontal"),
        icons::kFlipHorizontalIconPath,
        ARRAYSIZE(icons::kFlipHorizontalIconPath),
        icons::kToolbarIconViewport)));
    flip_vertical_button_ = static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(
        Metadata(flip_vertical_id_, UiElementRole::Button, AppAction::FlipVertical, L"Flip Vertical", L"Flip vertical", L"flip-vertical"),
        icons::kFlipVerticalIconPath,
        ARRAYSIZE(icons::kFlipVerticalIconPath),
        icons::kToolbarIconViewport)));
    reset_button_ =
        static_cast<IconButton*>(root_->AddChild(std::make_unique<IconButton>(
            Metadata(reset_view_id_, UiElementRole::Button, AppAction::ResetView, L"Reset View", L"Reset view", L"reset-view"),
            kResetIcon)));
    toolbar_drag_handle_ = root_->AddChild(std::make_unique<UiElement>(Metadata(
        toolbar_drag_handle_id_,
        UiElementRole::Pane,
        AppAction::None,
        L"Toolbar drag handle",
        L"",
        L"toolbar-drag-handle",
        false,
        false)));
    previous_button_->SetEnabled(false);
    next_button_->SetEnabled(false);
}

UiElement* ImageViewerUi::Root()
{
    return root_.get();
}

const UiElement* ImageViewerUi::Root() const
{
    return root_.get();
}

void ImageViewerUi::Draw(
    ID2D1DeviceContext* d2d_context,
    D2D1_SIZE_F viewport_size,
    IDWriteFactory* dwrite_factory,
    IDWriteTextFormat* body_text_format,
    IDWriteTextFormat* icon_text_format,
    ImageViewerUiState state)
{
    const UiDrawContext draw_context{
        .d2d_context = d2d_context,
        .body_text_format = body_text_format,
        .icon_text_format = icon_text_format,
    };
    const UiDraw draw(draw_context);
    Layout(viewport_size, dwrite_factory, body_text_format);
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
                (std::max)(ui_theme::metrics::kWindowBorderMinimum,
                    viewport_size.width - ui_theme::metrics::kWindowBorderInset),
                (std::max)(ui_theme::metrics::kWindowBorderMinimum,
                    viewport_size.height - ui_theme::metrics::kWindowBorderInset)),
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

    top_most_button_->Draw(draw_context, ButtonState(top_most_id_, state, top_most_));
    minimize_button_->Draw(draw_context, ButtonState(minimize_id_, state));
    maximize_button_->Draw(draw_context, ButtonState(maximize_restore_id_, state));
    close_button_->Draw(draw_context, ButtonState(close_id_, state, false, true));

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
    previous_button_->Draw(draw_context, ButtonState(previous_image_id_, state));
    next_button_->Draw(draw_context, ButtonState(next_image_id_, state));
    zoom_in_button_->Draw(draw_context, ButtonState(zoom_in_id_, state));
    zoom_out_button_->Draw(draw_context, ButtonState(zoom_out_id_, state));
    rotate_button_->Draw(draw_context, ButtonState(rotate_clockwise_id_, state));
    flip_horizontal_button_->Draw(draw_context, ButtonState(flip_horizontal_id_, state));
    flip_vertical_button_->Draw(draw_context, ButtonState(flip_vertical_id_, state));
    reset_button_->Draw(draw_context, ButtonState(reset_view_id_, state));
}

bool ImageViewerUi::IsPointInCaptionDragArea(D2D1_POINT_2F point) const
{
    const UiElement* hit_element = root_->HitTest(point);
    const UiElementId hit_id = hit_element != nullptr ? hit_element->Id() : UiElementId::None;
    return math::Contains(titlebar_rect_, point) && hit_id == UiElementId::None;
}

UiEventResult ImageViewerUi::OnPointerEvent(const UiPointerEvent& event)
{
    if (event.target != toolbar_drag_handle_id_ && event.captured != toolbar_drag_handle_id_) {
        return {};
    }

    return OnToolbarDragHandlePointerEvent(event);
}

UiEventResult ImageViewerUi::OnToolbarDragHandlePointerEvent(const UiPointerEvent& event)
{
    if (event.type == UiEventType::PointerDown && event.button == UiPointerButton::Left) {
        BeginToolbarDrag(event.point);
        return UiEventResult{
            .handled = true,
            .capture = UiCaptureRequest::Capture,
            .focus_target = toolbar_drag_handle_id_,
        };
    }

    if (event.type == UiEventType::PointerMove && toolbar_dragging_) {
        DragToolbar(event.point);
        return UiEventResult{
            .handled = true,
            .needs_render = true,
        };
    }

    if (event.type == UiEventType::PointerUp && toolbar_dragging_) {
        EndToolbarDrag();
        return UiEventResult{
            .handled = true,
            .needs_render = true,
            .capture = UiCaptureRequest::Release,
        };
    }

    return {};
}

void ImageViewerUi::BeginToolbarDrag(D2D1_POINT_2F point)
{
    toolbar_dragging_ = true;
    toolbar_drag_offset_ = D2D1::Point2F(point.x - toolbar_position_.x, point.y - toolbar_position_.y);
}

void ImageViewerUi::DragToolbar(D2D1_POINT_2F point)
{
    toolbar_position_ = D2D1::Point2F(point.x - toolbar_drag_offset_.x, point.y - toolbar_drag_offset_.y);
    toolbar_rect_ = D2D1::RectF(
        toolbar_position_.x,
        toolbar_position_.y,
        toolbar_position_.x + math::RectWidth(toolbar_rect_),
        toolbar_position_.y + math::RectHeight(toolbar_rect_));
}

void ImageViewerUi::EndToolbarDrag()
{
    toolbar_dragging_ = false;
}

void ImageViewerUi::SetTitleText(const wchar_t* title)
{
    title_text_ = title != nullptr && title[0] != L'\0' ? title : L"ImgViewer";
}

void ImageViewerUi::SetWindowState(bool top_most, bool maximized)
{
    top_most_ = top_most;
    maximized_ = maximized;
}

void ImageViewerUi::Layout(D2D1_SIZE_F viewport_size, IDWriteFactory*, IDWriteTextFormat*)
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
    toolbar_drag_handle_->SetRect(D2D1::RectF(
        toolbar_rect_.left,
        toolbar_rect_.top,
        toolbar_rect_.left + ui_theme::metrics::kToolbarPadding + ui_theme::metrics::kToolbarDragHandleWidth,
        toolbar_rect_.bottom));

    const std::vector<D2D1_RECT_F> toolbar_buttons = ui_layout::PlaceHorizontalRow(
        D2D1::Point2F(
            toolbar_rect_.left + ui_theme::metrics::kToolbarPadding + ui_theme::metrics::kToolbarDragHandleWidth,
            toolbar_rect_.top + ui_theme::metrics::kToolbarPadding),
        ui_theme::metrics::kToolbarButtonSize,
        std::vector<float>(toolbar_button_count, ui_theme::metrics::kToolbarButtonSize),
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

void ImageViewerUi::ClampToolbarToViewport(D2D1_SIZE_F viewport_size)
{
    const float toolbar_width = math::RectWidth(toolbar_rect_);
    const float toolbar_height = math::RectHeight(toolbar_rect_);
    const float max_left = (std::max)(0.0f, viewport_size.width - toolbar_width);
    const float max_top = (std::max)(0.0f, viewport_size.height - toolbar_height);
    const float left = std::clamp(toolbar_rect_.left, 0.0f, max_left);
    const float top = std::clamp(toolbar_rect_.top, 0.0f, max_top);
    toolbar_rect_ = D2D1::RectF(left, top, left + toolbar_width, top + toolbar_height);
}

UiElementState ImageViewerUi::ButtonState(UiElementId id, ImageViewerUiState state, bool active, bool danger) const
{
    const UiElement* element = root_->FindById(id);
    return UiElementState{
        .hovered = state.hovered == id,
        .pressed = state.pressed == id,
        .active = active,
        .danger = danger,
        .enabled = element != nullptr && element != root_.get() && element->IsEnabled(),
    };
}
