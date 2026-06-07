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

std::unique_ptr<IconButton> CreateButton(const ButtonSpec& spec, UiElementMetadata metadata)
{
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

ImgViewerUiToolbar::ImgViewerUiToolbar(UiElement& root)
{
    button_panel_ = static_cast<StackPanel*>(root.AddChild(std::make_unique<StackPanel>(
        UiMetadata(UiElementRole::Pane, kUiActionNone, L"Toolbar buttons", L"", L"toolbar-buttons", false, false),
        ui_layout::StackDirection::Horizontal)));
    button_panel_->SetGap(ui_theme::metrics::kToolbarButtonGap);
    for (const ButtonSpec& spec : kButtonSpecs) {
        ButtonInstance& button = buttons_[ButtonIndex(spec.button)];
        std::unique_ptr<IconButton> element = CreateButton(
            spec,
            UiMetadata(
                UiElementRole::Button,
                UiActionFromImgViewerAction(spec.action),
                spec.name,
                spec.tooltip,
                spec.automation_id));
        element->SetVisualDanger(spec.danger);
        button.element = button_panel_->AddItem(std::move(element), ui_theme::metrics::kToolbarButtonSize);
        button.id = button.element->Id();
        button.element->SetEnabled(spec.initially_enabled);
    }

    drag_handle_ = root.AddChild(std::make_unique<UiElement>(UiMetadata(
        UiElementRole::Pane,
        kUiActionNone,
        L"Toolbar drag handle",
        L"",
        L"toolbar-drag-handle",
        false,
        false)));
    drag_handle_id_ = drag_handle_->Id();
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

D2D1_SIZE_F ImgViewerUiToolbar::Measure(const UiDrawContext&, D2D1_SIZE_F) const
{
    const ToolbarMetrics metrics = MetricsForScale(scale_percent_);
    const float toolbar_content_width =
        metrics.button_size * static_cast<float>(kButtonSpecs.size()) +
        metrics.button_gap * static_cast<float>(kButtonSpecs.size() - 1);
    return D2D1::SizeF(
        toolbar_content_width + metrics.padding * 2.0f + metrics.drag_handle_width,
        metrics.button_size + metrics.padding * 2.0f);
}

void ImgViewerUiToolbar::Arrange(D2D1_RECT_F final_rect)
{
    const ToolbarMetrics metrics = MetricsForScale(scale_percent_);
    const D2D1_SIZE_F toolbar_size = Measure(UiDrawContext{}, D2D1::SizeF());
    if (!position_initialized_) {
        toolbar_position_ = D2D1::Point2F(
            (std::max)(0.0f, (final_rect.right - final_rect.left - toolbar_size.width) * 0.5f),
            (std::max)(0.0f, final_rect.bottom - toolbar_size.height - metrics.bottom_margin));
        position_initialized_ = true;
    }
    toolbar_rect_ = D2D1::RectF(
        toolbar_position_.x,
        toolbar_position_.y,
        toolbar_position_.x + toolbar_size.width,
        toolbar_position_.y + toolbar_size.height);
    ClampToViewport(D2D1::SizeF(final_rect.right - final_rect.left, final_rect.bottom - final_rect.top));
    toolbar_position_ = D2D1::Point2F(toolbar_rect_.left, toolbar_rect_.top);
    drag_handle_->Arrange(D2D1::RectF(
        toolbar_rect_.left,
        toolbar_rect_.top,
        toolbar_rect_.left + metrics.padding + metrics.drag_handle_width,
        toolbar_rect_.bottom));

    button_panel_->SetGap(metrics.button_gap);
    for (size_t index = 0; index < kButtonSpecs.size(); ++index) {
        button_panel_->SetItemFixedMainSize(index, metrics.button_size);
    }
    button_panel_->Measure(UiDrawContext{}, D2D1::SizeF(
        metrics.button_size * static_cast<float>(kButtonSpecs.size()) +
            metrics.button_gap * static_cast<float>(kButtonSpecs.size() - 1),
        metrics.button_size));
    button_panel_->Arrange(D2D1::RectF(
        toolbar_rect_.left + metrics.padding + metrics.drag_handle_width,
        toolbar_rect_.top + metrics.padding,
        toolbar_rect_.right - metrics.padding,
        toolbar_rect_.top + metrics.padding + metrics.button_size));
}

void ImgViewerUiToolbar::Render(
    const UiDrawContext& draw_context,
    UiRootState state,
    bool color_picker_active)
{
    const UiDraw draw(draw_context);
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
    draw.DrawRoundedRect(toolbar_background, ui_theme::color::kBorder, ui_theme::metrics::kStrokeWidth);

    Button(ButtonKey::ColorPicker)->SetVisualActive(color_picker_active);
    button_panel_->Render(draw_context, state);
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

void ImgViewerUiToolbar::RenderButton(
    ButtonKey button,
    const UiDrawContext& draw_context,
    UiRootState state,
    bool active,
    bool danger)
{
    const ButtonSpec& spec = kButtonSpecs[ButtonIndex(button)];
    Button(button)->SetVisualActive(active);
    Button(button)->SetVisualDanger(danger || spec.danger);
    Button(button)->Render(draw_context, state);
}
