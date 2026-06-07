#include "ui.slider.hpp"

#include <algorithm>
#include <memory>

#include <d2d1helper.h>

#include "math.hpp"
#include "ui.theme.hpp"

namespace {

constexpr float kSliderTrackHeight = 8.0f;
constexpr float kSliderThumbSize = 22.0f;

} // namespace

Slider::Slider(UiElementMetadata metadata, int minimum, int maximum, int value, int small_step, int large_step) :
    UiElement(metadata),
    minimum_((std::min)(minimum, maximum)),
    maximum_((std::max)(minimum, maximum)),
    small_step_((std::max)(1, small_step)),
    large_step_((std::max)(1, large_step))
{
    SetFocusable(true);
    SetValue(value);
}

int Slider::Minimum() const
{
    return minimum_;
}

int Slider::Maximum() const
{
    return maximum_;
}

int Slider::Value() const
{
    return value_;
}

bool Slider::SetValue(int value)
{
    const int clamped = (std::clamp)(value, minimum_, maximum_);
    if (clamped == value_) {
        return false;
    }
    value_ = clamped;
    return true;
}

D2D1_SIZE_F Slider::Measure(const UiDrawContext&, D2D1_SIZE_F available_size) const
{
    return D2D1::SizeF((std::max)(1.0f, available_size.width), 36.0f);
}

void Slider::Render(const UiDrawContext& context, UiRootState root_state) const
{
    const UiElementState state = VisualState(root_state);
    const UiDraw draw(context);
    const D2D1_RECT_F track = TrackRect();
    const D2D1_RECT_F thumb = ThumbRect();
    const float center_y = (track.top + track.bottom) * 0.5f;
    const D2D1_RECT_F fill = D2D1::RectF(track.left, track.top, (thumb.left + thumb.right) * 0.5f, track.bottom);

    draw.FillRoundedRect(D2D1::RoundedRect(track, kSliderTrackHeight * 0.5f, kSliderTrackHeight * 0.5f), ui_theme::color::kButtonDisabled);
    draw.FillRoundedRect(D2D1::RoundedRect(fill, kSliderTrackHeight * 0.5f, kSliderTrackHeight * 0.5f), ui_theme::color::kAccent);
    draw.DrawRect(D2D1::RectF(track.left, center_y - 0.5f, track.right, center_y + 0.5f), ui_theme::color::kBorder);

    const D2D1_COLOR_F thumb_fill = !state.enabled
        ? ui_theme::color::kButtonDisabled
        : state.pressed ? ui_theme::color::kButtonPressed : state.hovered || state.active ? ui_theme::color::kButtonHovered : ui_theme::color::kButtonDefault;
    draw.FillRoundedRect(D2D1::RoundedRect(thumb, kSliderThumbSize * 0.5f, kSliderThumbSize * 0.5f), thumb_fill);
    draw.DrawRoundedRect(
        D2D1::RoundedRect(thumb, kSliderThumbSize * 0.5f, kSliderThumbSize * 0.5f),
        state.active ? ui_theme::color::kAccent : ui_theme::color::kBorder,
        state.active ? 1.5f : 1.0f);
}

UiEventResult Slider::OnInputEvent(const UiInputEvent& event)
{
    switch (event.type) {
    case UiEventType::PointerMove:
    case UiEventType::PointerDown:
    case UiEventType::PointerUp:
    case UiEventType::PointerLeave:
        return OnPointerEvent(event.pointer);
    case UiEventType::KeyDown:
    case UiEventType::KeyUp:
        return OnKeyEvent(event.key);
    default:
        return {};
    }
}

UiEventResult Slider::OnPointerEvent(const UiPointerEvent& event)
{
    if (!IsEnabled()) {
        return {};
    }

    if (event.type == UiEventType::PointerDown && event.button == UiPointerButton::Left) {
        dragging_ = true;
        const bool changed = SetValue(ValueFromPoint(event.point));
        return UiEventResult{
            .handled = true,
            .needs_render = true,
            .capture = UiCaptureRequest::Capture,
            .focus = UiFocusRequest::FocusTarget,
            .focus_target = Id(),
            .value_changed = changed,
        };
    }

    if (event.type == UiEventType::PointerMove && dragging_ && event.captured == Id()) {
        const bool changed = SetValue(ValueFromPoint(event.point));
        return UiEventResult{.handled = true, .needs_render = changed, .value_changed = changed};
    }

    if (event.type == UiEventType::PointerUp && event.captured == Id()) {
        dragging_ = false;
        const bool changed = SetValue(ValueFromPoint(event.point));
        return UiEventResult{
            .handled = true,
            .needs_render = true,
            .capture = UiCaptureRequest::Release,
            .value_changed = changed,
        };
    }

    return {};
}

UiEventResult Slider::OnKeyEvent(const UiKeyEvent& event)
{
    if (event.type != UiEventType::KeyDown || !IsEnabled()) {
        return {};
    }

    int next = value_;
    switch (event.virtual_key) {
    case VK_LEFT:
    case VK_DOWN:
        next -= small_step_;
        break;
    case VK_RIGHT:
    case VK_UP:
        next += small_step_;
        break;
    case VK_PRIOR:
        next += large_step_;
        break;
    case VK_NEXT:
        next -= large_step_;
        break;
    case VK_HOME:
        next = minimum_;
        break;
    case VK_END:
        next = maximum_;
        break;
    default:
        return {};
    }

    const bool changed = SetValue(next);
    return UiEventResult{.handled = true, .needs_render = changed, .value_changed = changed};
}

int Slider::ValueFromPoint(D2D1_POINT_2F point) const
{
    const D2D1_RECT_F track = TrackRect();
    const float width = (std::max)(1.0f, math::RectWidth(track));
    const float ratio = (std::clamp)((point.x - track.left) / width, 0.0f, 1.0f);
    return minimum_ + static_cast<int>(ratio * static_cast<float>(maximum_ - minimum_) + 0.5f);
}

D2D1_RECT_F Slider::TrackRect() const
{
    const D2D1_RECT_F rect = Rect();
    const float center_y = (rect.top + rect.bottom) * 0.5f;
    return D2D1::RectF(
        rect.left + kSliderThumbSize * 0.5f,
        center_y - kSliderTrackHeight * 0.5f,
        rect.right - kSliderThumbSize * 0.5f,
        center_y + kSliderTrackHeight * 0.5f);
}

D2D1_RECT_F Slider::ThumbRect() const
{
    const D2D1_RECT_F track = TrackRect();
    const float ratio = maximum_ == minimum_
        ? 0.0f
        : static_cast<float>(value_ - minimum_) / static_cast<float>(maximum_ - minimum_);
    const float center_x = track.left + math::RectWidth(track) * ratio;
    const float center_y = (track.top + track.bottom) * 0.5f;
    return D2D1::RectF(
        center_x - kSliderThumbSize * 0.5f,
        center_y - kSliderThumbSize * 0.5f,
        center_x + kSliderThumbSize * 0.5f,
        center_y + kSliderThumbSize * 0.5f);
}

// SliderRow

SliderRow::SliderRow(UiElementMetadata metadata, int minimum, int maximum, int value,
                     int small_step, int large_step, float value_width) :
    UiElement(metadata),
    value_width_(value_width)
{
    auto slider = std::make_unique<Slider>(
        UiMetadata(UiElementRole::Slider, metadata.action, metadata.name,
                   metadata.tooltip, metadata.automation_id),
        minimum, maximum, value, small_step, large_step);
    slider_ = static_cast<Slider*>(AddChild(std::move(slider)));
}

int SliderRow::Value() const
{
    return slider_->Value();
}

bool SliderRow::SetValue(int value)
{
    return slider_->SetValue(value);
}

void SliderRow::SetValueText(const wchar_t* text)
{
    value_text_ = text;
}

Slider* SliderRow::GetSlider()
{
    return slider_;
}

D2D1_SIZE_F SliderRow::Measure(const UiDrawContext&, D2D1_SIZE_F available_size) const
{
    return D2D1::SizeF((std::max)(1.0f, available_size.width), 36.0f);
}

void SliderRow::Arrange(D2D1_RECT_F final_rect)
{
    UiElement::Arrange(final_rect);
    const float slider_right = final_rect.right - value_width_ - kGap;
    slider_->Arrange(D2D1::RectF(final_rect.left, final_rect.top, slider_right, final_rect.bottom));
    value_rect_ = D2D1::RectF(slider_right + kGap, final_rect.top, final_rect.right, final_rect.bottom);
}

void SliderRow::Render(const UiDrawContext& context, UiRootState state) const
{
    slider_->Render(context, state);
    if (!value_text_.empty()) {
        const UiDraw draw(context);
        draw.DrawBodyText(
            value_text_.c_str(),
            static_cast<UINT32>(value_text_.size()),
            value_rect_,
            ui_theme::color::kBodyText,
            D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    }
}

UiEventResult SliderRow::OnInputEvent(const UiInputEvent& event)
{
    return slider_->OnInputEvent(event);
}
