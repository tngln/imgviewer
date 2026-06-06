#include "imgviewer.ui.toolbar.hpp"

#include "imgviewer.config.hpp"
#include "imgviewer.ui.hpp"

#include <algorithm>
#include <memory>
#include <vector>

#include <d2d1helper.h>

#include "imgviewer.ui.action.hpp"
#include "math.hpp"
#include "ui.layout.hpp"
#include "ui.theme.hpp"

namespace {

constexpr wchar_t kPreviousIcon[] = L"\xE892";
constexpr wchar_t kNextIcon[] = L"\xE893";
constexpr wchar_t kZoomInIcon[] = L"\xE8A3";
constexpr wchar_t kZoomOutIcon[] = L"\xE71F";
constexpr wchar_t kRotateIcon[] = L"\xE7AD";
constexpr wchar_t kResetIcon[] = L"\xE777";

enum class ButtonIconKind {
    Glyph,
    Path,
};

struct ButtonIconSpec final {
    ButtonIconKind kind = ButtonIconKind::Glyph;
    const wchar_t* glyph = L"";
    const icons::PathIcon* path_icon = nullptr;
};

struct ButtonSpec final {
    ImgViewerUiToolbar::ButtonKey button = ImgViewerUiToolbar::ButtonKey::PreviousImage;
    ImgViewerAction action = ImgViewerAction::None;
    const wchar_t* name = L"";
    const wchar_t* tooltip = L"";
    const wchar_t* automation_id = L"";
    ButtonIconSpec icon = {};
    bool initially_enabled = true;
    bool danger = false;
};

struct ToolbarMetrics final {
    float button_size = ui_theme::metrics::kToolbarButtonSize;
    float button_gap = ui_theme::metrics::kToolbarButtonGap;
    float padding = ui_theme::metrics::kToolbarPadding;
    float drag_handle_width = ui_theme::metrics::kToolbarDragHandleWidth;
    float bottom_margin = ui_theme::metrics::kToolbarBottomMargin;
    float corner_radius = ui_theme::metrics::kToolbarCornerRadius;
};

ToolbarMetrics MetricsForScale(int percent)
{
    const float scale = static_cast<float>(ClampToolbarScalePercent(percent)) / 100.0f;
    return ToolbarMetrics{
        .button_size = ui_theme::metrics::kToolbarButtonSize * scale,
        .button_gap = ui_theme::metrics::kToolbarButtonGap * scale,
        .padding = ui_theme::metrics::kToolbarPadding * scale,
        .drag_handle_width = ui_theme::metrics::kToolbarDragHandleWidth * scale,
        .bottom_margin = ui_theme::metrics::kToolbarBottomMargin * scale,
        .corner_radius = ui_theme::metrics::kToolbarCornerRadius * scale,
    };
}

constexpr ButtonIconSpec GlyphIcon(const wchar_t* glyph)
{
    return ButtonIconSpec{
        .kind = ButtonIconKind::Glyph,
        .glyph = glyph,
    };
}

constexpr ButtonIconSpec PathIcon(const icons::PathIcon& icon)
{
    return ButtonIconSpec{
        .kind = ButtonIconKind::Path,
        .path_icon = &icon,
    };
}

UiElementMetadata Metadata(
    UiElementId id,
    UiElementRole role,
    UiAction action,
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

std::unique_ptr<IconButton> CreateButton(const ButtonSpec& spec, UiElementId id)
{
    const UiElementMetadata metadata =
        Metadata(
            id,
            UiElementRole::Button,
            UiActionFromImgViewerAction(spec.action),
            spec.name,
            spec.tooltip,
            spec.automation_id);
    if (spec.icon.kind == ButtonIconKind::Path) {
        return std::make_unique<IconButton>(metadata, *spec.icon.path_icon);
    }

    return std::make_unique<IconButton>(metadata, spec.icon.glyph);
}

constexpr std::array<ButtonSpec, ImgViewerUiToolbar::kButtonCount> kButtonSpecs{{
    {ImgViewerUiToolbar::ButtonKey::PreviousImage, ImgViewerAction::PreviousImage, L"Previous Image", L"Previous image",
        L"previous-image", GlyphIcon(kPreviousIcon), false},
    {ImgViewerUiToolbar::ButtonKey::NextImage, ImgViewerAction::NextImage, L"Next Image", L"Next image", L"next-image",
        GlyphIcon(kNextIcon), false},
    {ImgViewerUiToolbar::ButtonKey::ZoomIn, ImgViewerAction::ZoomIn, L"Zoom In", L"Zoom in", L"zoom-in",
        GlyphIcon(kZoomInIcon)},
    {ImgViewerUiToolbar::ButtonKey::ZoomOut, ImgViewerAction::ZoomOut, L"Zoom Out", L"Zoom out", L"zoom-out",
        GlyphIcon(kZoomOutIcon)},
    {ImgViewerUiToolbar::ButtonKey::FitWindow, ImgViewerAction::FitWindow, L"Fit Window", L"Fit window", L"fit-window",
        PathIcon(icons::kFitWindowIcon)},
    {ImgViewerUiToolbar::ButtonKey::ActualSize, ImgViewerAction::ActualSize, L"Actual Size", L"Actual size", L"actual-size",
        PathIcon(icons::kActualSizeIcon)},
    {ImgViewerUiToolbar::ButtonKey::RotateClockwise, ImgViewerAction::RotateClockwise, L"Rotate Clockwise",
        L"Rotate clockwise", L"rotate-clockwise", GlyphIcon(kRotateIcon)},
    {ImgViewerUiToolbar::ButtonKey::FlipHorizontal, ImgViewerAction::FlipHorizontal, L"Flip Horizontal", L"Flip horizontal",
        L"flip-horizontal", PathIcon(icons::kFlipHorizontalIcon)},
    {ImgViewerUiToolbar::ButtonKey::FlipVertical, ImgViewerAction::FlipVertical, L"Flip Vertical", L"Flip vertical",
        L"flip-vertical", PathIcon(icons::kFlipVerticalIcon)},
    {ImgViewerUiToolbar::ButtonKey::ResetView, ImgViewerAction::ResetView, L"Reset View", L"Reset view", L"reset-view",
        GlyphIcon(kResetIcon)},
    {ImgViewerUiToolbar::ButtonKey::ColorPicker, ImgViewerAction::ToggleColorPicker, L"Color Picker", L"Color picker",
        L"color-picker", PathIcon(icons::kColorPickerIcon), false},
}};

constexpr bool ButtonSpecsMatchKeys()
{
    for (size_t index = 0; index < kButtonSpecs.size(); ++index) {
        if (static_cast<size_t>(kButtonSpecs[index].button) != index) {
            return false;
        }
    }

    return true;
}

static_assert(ButtonSpecsMatchKeys());

} // namespace

constexpr size_t ImgViewerUiToolbar::ButtonIndex(ButtonKey button)
{
    return static_cast<size_t>(button);
}

ImgViewerUiToolbar::ImgViewerUiToolbar(UiElement& root, UiElementIdGenerator& ids)
{
    for (const ButtonSpec& spec : kButtonSpecs) {
        ButtonInstance& button = buttons_[ButtonIndex(spec.button)];
        button.id = ids.Next();
        button.element = static_cast<IconButton*>(root.AddChild(CreateButton(spec, button.id)));
        button.element->SetEnabled(spec.initially_enabled);
    }

    drag_handle_id_ = ids.Next();
    drag_handle_ = root.AddChild(std::make_unique<UiElement>(Metadata(
        drag_handle_id_,
        UiElementRole::Pane,
        kUiActionNone,
        L"Toolbar drag handle",
        L"",
        L"toolbar-drag-handle",
        false,
        false)));
    SetScalePercent(scale_percent_);
}

void ImgViewerUiToolbar::SetScalePercent(int percent)
{
    scale_percent_ = ClampToolbarScalePercent(percent);
    const float icon_scale = static_cast<float>(scale_percent_) / 100.0f;
    for (const ButtonInstance& button : buttons_) {
        if (button.element != nullptr) {
            button.element->SetIconScale(icon_scale);
        }
    }
}

void ImgViewerUiToolbar::Draw(
    const UiDrawContext& draw_context,
    UiRootState state,
    bool color_picker_active)
{
    const UiDraw draw(draw_context);
    Layout(draw_context.viewport_size);
    const ToolbarMetrics metrics = MetricsForScale(scale_percent_);

    const D2D1_ROUNDED_RECT toolbar_background = D2D1::RoundedRect(
        toolbar_rect_,
        metrics.corner_radius,
        metrics.corner_radius);
    draw.FillRoundedRect(
        toolbar_background,
        D2D1::ColorF(
            ui_theme::color::kToolbarBackground.r,
            ui_theme::color::kToolbarBackground.g,
            ui_theme::color::kToolbarBackground.b,
            ui_theme::color::kToolbarBackgroundOpacity));
    draw.DrawRoundedRect(toolbar_background, ui_theme::color::kBorder, 1.0f);

    for (const ButtonSpec& spec : kButtonSpecs) {
        DrawButton(spec.button, draw_context, state, spec.button == ButtonKey::ColorPicker && color_picker_active);
    }
}

void ImgViewerUiToolbar::Layout(D2D1_SIZE_F viewport_size)
{
    const ToolbarMetrics metrics = MetricsForScale(scale_percent_);
    const float toolbar_content_width =
        metrics.button_size * static_cast<float>(kButtonSpecs.size()) +
        metrics.button_gap * static_cast<float>(kButtonSpecs.size() - 1);
    const float toolbar_width = toolbar_content_width +
        metrics.padding * 2.0f +
        metrics.drag_handle_width;
    const float toolbar_height = metrics.button_size + metrics.padding * 2.0f;
    if (!position_initialized_) {
        toolbar_position_ = D2D1::Point2F(
            (std::max)(0.0f, (viewport_size.width - toolbar_width) * 0.5f),
            (std::max)(0.0f, viewport_size.height - toolbar_height - metrics.bottom_margin));
        position_initialized_ = true;
    }
    toolbar_rect_ = D2D1::RectF(
        toolbar_position_.x,
        toolbar_position_.y,
        toolbar_position_.x + toolbar_width,
        toolbar_position_.y + toolbar_height);
    ClampToViewport(viewport_size);
    toolbar_position_ = D2D1::Point2F(toolbar_rect_.left, toolbar_rect_.top);
    drag_handle_->SetRect(D2D1::RectF(
        toolbar_rect_.left,
        toolbar_rect_.top,
        toolbar_rect_.left + metrics.padding + metrics.drag_handle_width,
        toolbar_rect_.bottom));

    const std::vector<D2D1_RECT_F> toolbar_buttons = ui_layout::PlaceHorizontalRow(
        D2D1::Point2F(
            toolbar_rect_.left + metrics.padding + metrics.drag_handle_width,
            toolbar_rect_.top + metrics.padding),
        metrics.button_size,
        std::vector<float>(kButtonSpecs.size(), metrics.button_size),
        metrics.button_gap);
    for (const ButtonSpec& spec : kButtonSpecs) {
        Button(spec.button)->SetRect(toolbar_buttons[ButtonIndex(spec.button)]);
    }
}

void ImgViewerUiToolbar::ClampToViewport(D2D1_SIZE_F viewport_size)
{
    const float toolbar_width = math::RectWidth(toolbar_rect_);
    const float toolbar_height = math::RectHeight(toolbar_rect_);
    const float max_left = (std::max)(0.0f, viewport_size.width - toolbar_width);
    const float max_top = (std::max)(0.0f, viewport_size.height - toolbar_height);
    const float left = std::clamp(toolbar_rect_.left, 0.0f, max_left);
    const float top = std::clamp(toolbar_rect_.top, 0.0f, max_top);
    toolbar_rect_ = D2D1::RectF(left, top, left + toolbar_width, top + toolbar_height);
}

UiEventResult ImgViewerUiToolbar::OnPointerEvent(const UiPointerEvent& event)
{
    if (event.target != drag_handle_id_ && event.captured != drag_handle_id_) {
        return {};
    }

    return OnDragHandlePointerEvent(event);
}

UiEventResult ImgViewerUiToolbar::OnDragHandlePointerEvent(const UiPointerEvent& event)
{
    if (event.type == UiEventType::PointerDown && event.button == UiPointerButton::Left) {
        BeginDrag(event.point);
        return UiEventResult{
            .handled = true,
            .capture = UiCaptureRequest::Capture,
            .focus_target = drag_handle_id_,
        };
    }

    if (event.type == UiEventType::PointerMove && dragging_) {
        Drag(event.point);
        return UiEventResult{
            .handled = true,
            .needs_render = true,
        };
    }

    if (event.type == UiEventType::PointerUp && dragging_) {
        EndDrag();
        return UiEventResult{
            .handled = true,
            .needs_render = true,
            .capture = UiCaptureRequest::Release,
        };
    }

    return {};
}

void ImgViewerUiToolbar::BeginDrag(D2D1_POINT_2F point)
{
    dragging_ = true;
    drag_offset_ = D2D1::Point2F(point.x - toolbar_position_.x, point.y - toolbar_position_.y);
}

void ImgViewerUiToolbar::Drag(D2D1_POINT_2F point)
{
    toolbar_position_ = D2D1::Point2F(point.x - drag_offset_.x, point.y - drag_offset_.y);
    toolbar_rect_ = D2D1::RectF(
        toolbar_position_.x,
        toolbar_position_.y,
        toolbar_position_.x + math::RectWidth(toolbar_rect_),
        toolbar_position_.y + math::RectHeight(toolbar_rect_));
}

void ImgViewerUiToolbar::EndDrag()
{
    dragging_ = false;
}

IconButton* ImgViewerUiToolbar::Button(ButtonKey button)
{
    return buttons_[ButtonIndex(button)].element;
}

const IconButton* ImgViewerUiToolbar::Button(ButtonKey button) const
{
    return buttons_[ButtonIndex(button)].element;
}

UiElementState ImgViewerUiToolbar::ButtonState(ButtonKey button, UiRootState state, bool active, bool danger) const
{
    const ButtonInstance& instance = buttons_[ButtonIndex(button)];
    return UiElementState{
        .hovered = state.hovered == instance.id,
        .pressed = state.pressed == instance.id,
        .active = active,
        .danger = danger,
        .enabled = instance.element != nullptr && instance.element->IsEnabled(),
    };
}

void ImgViewerUiToolbar::DrawButton(
    ButtonKey button,
    const UiDrawContext& draw_context,
    UiRootState state,
    bool active,
    bool danger) const
{
    const ButtonSpec& spec = kButtonSpecs[ButtonIndex(button)];
    Button(button)->Draw(draw_context, ButtonState(button, state, active, danger || spec.danger));
}
