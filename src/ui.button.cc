#include "ui.button.hpp"

#include <algorithm>
#include <cmath>
#include <cwchar>

#include <d2d1helper.h>
#include <wil/com.h>

#include "ui.events.hpp"
#include "ui.text.hpp"
#include "ui.theme.hpp"

namespace {

constexpr float kPathIconRenderSize = 10.0f;
constexpr float kGlyphIconBoxSize = 12.0f;

class D2DTransformGuard final {
public:
    D2DTransformGuard(ID2D1RenderTarget* target, D2D1_MATRIX_3X2_F transform) : target_(target)
    {
        if (target_ == nullptr) {
            return;
        }

        target_->GetTransform(&old_transform_);
        target_->SetTransform(transform * old_transform_);
    }

    D2DTransformGuard(const D2DTransformGuard&) = delete;
    D2DTransformGuard& operator=(const D2DTransformGuard&) = delete;

    ~D2DTransformGuard()
    {
        if (target_ != nullptr) {
            target_->SetTransform(old_transform_);
        }
    }

private:
    ID2D1RenderTarget* target_ = nullptr;
    D2D1_MATRIX_3X2_F old_transform_ = D2D1::Matrix3x2F::Identity();
};

D2D1_POINT_2F RectCenter(D2D1_RECT_F rect)
{
    return D2D1::Point2F((rect.left + rect.right) * 0.5f, (rect.top + rect.bottom) * 0.5f);
}

UiEventResult ButtonPointerEvent(UiElement& button, const UiPointerEvent& event)
{
    if (event.button != UiPointerButton::Left &&
        (event.type == UiEventType::PointerDown || event.type == UiEventType::PointerUp)) {
        return {};
    }

    if (event.type == UiEventType::PointerDown) {
        const bool can_activate = button.IsEnabled();
        return UiEventResult{
            .handled = true,
            .needs_render = can_activate,
            .capture = can_activate ? UiCaptureRequest::Capture : UiCaptureRequest::None,
            .focus = can_activate && button.IsFocusable() ? UiFocusRequest::FocusTarget : UiFocusRequest::None,
            .focus_target = can_activate ? button.Id() : UiElementId::None,
        };
    }

    if (event.type == UiEventType::PointerUp && event.captured == button.Id()) {
        return UiEventResult{
            .handled = true,
            .needs_render = button.IsEnabled(),
            .capture = UiCaptureRequest::Release,
            .action = button.IsEnabled() && event.target == button.Id() ? button.Action() : kUiActionNone,
        };
    }

    return {};
}

UiEventResult ButtonKeyEvent(UiElement& button, const UiKeyEvent& event)
{
    if (event.type != UiEventType::KeyDown || !button.IsEnabled()) {
        return {};
    }

    if (event.virtual_key != VK_RETURN && event.virtual_key != VK_SPACE) {
        return {};
    }

    return UiEventResult{
        .handled = true,
        .needs_render = true,
        .action = button.Action(),
    };
}

} // namespace

Button::Button(UiElementMetadata metadata, const wchar_t* icon, const wchar_t* text) :
    UiElement(metadata),
    icon_(icon),
    text_(text)
{
    SetFocusable(true);
}

float Button::PreferredWidth(const UiDrawContext& context) const
{
    return PreferredWidth(context.dwrite_factory, context.body_text_format);
}

float Button::PreferredWidth(IDWriteFactory* factory, IDWriteTextFormat* body_text_format) const
{
    const ui_text::TextMetrics metrics =
        ui_text::MeasureText(factory, body_text_format, text_, static_cast<UINT32>(wcslen(text_)));
    return ui_theme::offset::kButtonTextLeft +
        std::ceil(metrics.width) +
        ui_theme::offset::kButtonTextRight;
}

D2D1_SIZE_F Button::Measure(const UiDrawContext& context, D2D1_SIZE_F) const
{
    return D2D1::SizeF(PreferredWidth(context), ui_theme::metrics::kPrimaryButtonHeight);
}

void Button::Render(const UiDrawContext& context, UiRootState root_state) const
{
    const UiElementState state = VisualState(root_state);
    const D2D1_RECT_F rect = Rect();
    const UiDraw draw(context);
    const D2D1_ROUNDED_RECT button =
        D2D1::RoundedRect(rect, ui_theme::metrics::kButtonCornerRadius, ui_theme::metrics::kButtonCornerRadius);
    draw.FillRoundedRect(button, ui_theme::WidgetFillColor(state));
    draw.DrawRoundedRect(button, ui_theme::color::kBorder, ui_theme::metrics::kStrokeWidth);
    const D2D1_COLOR_F content_color =
        state.enabled ? ui_theme::color::kAccent : ui_theme::color::kButtonDisabledContent;
    const D2D1_COLOR_F text_color =
        state.enabled ? ui_theme::color::kBodyText : ui_theme::color::kButtonDisabledContent;
    draw.DrawIconText(
        icon_,
        static_cast<UINT32>(wcslen(icon_)),
        D2D1::RectF(
            rect.left + ui_theme::offset::kButtonIconLeft,
            rect.top + ui_theme::offset::kButtonIconTop,
            rect.left + ui_theme::offset::kButtonIconRight,
            rect.bottom),
        content_color);
    draw.DrawBodyText(
        text_,
        static_cast<UINT32>(wcslen(text_)),
        D2D1::RectF(
            rect.left + ui_theme::offset::kButtonTextLeft,
            rect.top + ui_theme::offset::kButtonTextTop,
            rect.right - ui_theme::offset::kButtonTextRight,
            rect.bottom),
        text_color,
        D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
}

UiEventResult Button::OnPointerEvent(const UiPointerEvent& event)
{
    return ButtonPointerEvent(*this, event);
}

UiEventResult Button::OnKeyEvent(const UiKeyEvent& event)
{
    return ButtonKeyEvent(*this, event);
}

IconButton::IconButton(UiElementMetadata metadata, const wchar_t* icon) : UiElement(metadata), icon_(icon)
{
    SetFocusable(true);
}

IconButton::IconButton(
    UiElementMetadata metadata,
    const icons::PathIcon& icon) :
    UiElement(metadata),
    path_icon_(&icon)
{
    SetFocusable(true);
}

void IconButton::SetIcon(const wchar_t* icon)
{
    icon_ = icon;
    path_icon_ = nullptr;
}

void IconButton::SetIconScale(float scale)
{
    icon_scale_ = (std::clamp)(scale, 0.5f, 2.0f);
}

D2D1_SIZE_F IconButton::Measure(const UiDrawContext&, D2D1_SIZE_F) const
{
    return D2D1::SizeF(ui_theme::metrics::kCaptionButtonWidth, ui_theme::metrics::kTitleBarHeight);
}

void IconButton::Render(const UiDrawContext& context, UiRootState root_state) const
{
    const UiElementState state = VisualState(root_state);
    const D2D1_RECT_F rect = Rect();
    const D2D1_POINT_2F center = RectCenter(rect);
    const UiDraw draw(context);
    draw.FillRect(rect, ui_theme::WidgetFillColor(state));
    const D2D1_COLOR_F icon_color = !state.enabled
        ? ui_theme::color::kButtonDisabledContent
        : state.danger && state.hovered ? ui_theme::color::kBodyText : ui_theme::color::kAccent;
    if (path_icon_ != nullptr && path_icon_->command_count > 0 && context.d2d_context != nullptr) {
        wil::com_ptr<ID2D1Factory> factory;
        context.d2d_context->GetFactory(factory.put());
        wil::com_ptr<ID2D1PathGeometry> geometry;
        if (SUCCEEDED(CreatePathGeometryFromIcon(
                factory.get(),
                path_icon_->commands,
                path_icon_->command_count,
                geometry.put()))) {
            const float icon_size = kPathIconRenderSize;
            const float scaled_icon_size = icon_size * icon_scale_;
            const float icon_width = path_icon_->view_box.right - path_icon_->view_box.left;
            const float icon_height = path_icon_->view_box.bottom - path_icon_->view_box.top;
            const float icon_viewport = (std::max)(icon_width, icon_height);
            const float scale = scaled_icon_size / icon_viewport;
            const float left = center.x - icon_width * scale * 0.5f;
            const float top = center.y - icon_height * scale * 0.5f;
            const D2DTransformGuard transform_guard(
                context.d2d_context,
                D2D1::Matrix3x2F::Translation(-path_icon_->view_box.left, -path_icon_->view_box.top) *
                D2D1::Matrix3x2F::Scale(scale, scale) *
                D2D1::Matrix3x2F::Translation(left, top));
            draw.DrawGeometry(geometry.get(), icon_color, ui_theme::metrics::kPathIconStrokeWidth);
        }
        return;
    }

    const float icon_box_size = kGlyphIconBoxSize;
    const D2D1_RECT_F icon_rect = D2D1::RectF(
        center.x - icon_box_size * 0.5f,
        center.y - icon_box_size * 0.5f,
        center.x + icon_box_size * 0.5f,
        center.y + icon_box_size * 0.5f);
    const D2DTransformGuard transform_guard(
        icon_scale_ != 1.0f ? context.d2d_context : nullptr,
        D2D1::Matrix3x2F::Scale(icon_scale_, icon_scale_, center));
    draw.DrawIconText(
        icon_,
        static_cast<UINT32>(wcslen(icon_)),
        icon_rect,
        icon_color);
}

UiEventResult IconButton::OnPointerEvent(const UiPointerEvent& event)
{
    return ButtonPointerEvent(*this, event);
}

UiEventResult IconButton::OnKeyEvent(const UiKeyEvent& event)
{
    return ButtonKeyEvent(*this, event);
}
