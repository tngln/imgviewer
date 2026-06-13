#include "ui.panel.hpp"

#include <algorithm>

#include <d2d1helper.h>

#include "math.hpp"
#include "ui.events.hpp"
#include "ui.theme.hpp"

namespace {

float MainSize(D2D1_SIZE_F size, ui_layout::StackDirection direction)
{
    return direction == ui_layout::StackDirection::Horizontal ? size.width : size.height;
}

float CrossSize(D2D1_SIZE_F size, ui_layout::StackDirection direction)
{
    return direction == ui_layout::StackDirection::Horizontal ? size.height : size.width;
}

} // namespace

StackPanel::StackPanel(UiElementMetadata metadata, ui_layout::StackDirection direction) :
    UiElement(metadata),
    direction_(direction)
{
}

void StackPanel::SetGap(float gap)
{
    gap_ = (std::max)(0.0f, gap);
}

void StackPanel::SetPadding(UiThickness padding)
{
    padding_ = padding;
}

void StackPanel::SetCrossAxis(ui_layout::StackCrossAxis cross_axis)
{
    cross_axis_ = cross_axis;
}

void StackPanel::SetItemFixedMainSize(size_t index, float fixed_main_size)
{
    if (index >= item_fixed_main_sizes_.size()) {
        item_fixed_main_sizes_.resize(index + 1, 0.0f);
    }
    item_fixed_main_sizes_[index] = (std::max)(0.0f, fixed_main_size);
}

D2D1_SIZE_F StackPanel::Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const
{
    const float horizontal_padding = padding_.left + padding_.right;
    const float vertical_padding = padding_.top + padding_.bottom;
    const D2D1_SIZE_F child_available = D2D1::SizeF(
        (std::max)(0.0f, available_size.width - horizontal_padding),
        (std::max)(0.0f, available_size.height - vertical_padding));

    measured_children_.clear();
    measured_children_.reserve(ChildCount());

    float main = 0.0f;
    float cross = 0.0f;
    for (size_t index = 0; index < ChildCount(); ++index) {
        const UiElement* child = ChildAt(index);
        const D2D1_SIZE_F measured = child != nullptr ? child->Measure(context, child_available) : D2D1_SIZE_F{};
        measured_children_.push_back(measured);
        const float fixed = FixedMainSizeAt(index);
        main += fixed > 0.0f ? fixed : MainSize(measured, direction_);
        cross = (std::max)(cross, CrossSize(measured, direction_));
    }
    if (ChildCount() > 1) {
        main += gap_ * static_cast<float>(ChildCount() - 1);
    }

    return direction_ == ui_layout::StackDirection::Horizontal
        ? D2D1::SizeF(main + horizontal_padding, cross + vertical_padding)
        : D2D1::SizeF(cross + horizontal_padding, main + vertical_padding);
}

void StackPanel::Arrange(D2D1_RECT_F final_rect)
{
    UiElement::Arrange(final_rect);
    const D2D1_RECT_F content = ContentRect(final_rect);
    float offset = direction_ == ui_layout::StackDirection::Horizontal ? content.left : content.top;
    for (size_t index = 0; index < ChildCount(); ++index) {
        UiElement* child = ChildAt(index);
        if (child == nullptr) {
            continue;
        }

        const D2D1_SIZE_F measured = index < measured_children_.size() ? measured_children_[index] : D2D1_SIZE_F{};
        const float fixed = FixedMainSizeAt(index);
        const float main = fixed > 0.0f ? fixed : MainSize(measured, direction_);
        if (direction_ == ui_layout::StackDirection::Horizontal) {
            const float bottom = cross_axis_ == ui_layout::StackCrossAxis::Stretch
                ? content.bottom
                : content.top + CrossSize(measured, direction_);
            child->Arrange(D2D1::RectF(offset, content.top, offset + main, bottom));
        } else {
            const float right = cross_axis_ == ui_layout::StackCrossAxis::Stretch
                ? content.right
                : content.left + CrossSize(measured, direction_);
            child->Arrange(D2D1::RectF(content.left, offset, right, offset + main));
        }
        offset += main + gap_;
    }
}

void StackPanel::Render(const UiDrawContext& context, UiRootState state) const
{
    for (size_t index = 0; index < ChildCount(); ++index) {
        const UiElement* child = ChildAt(index);
        if (child != nullptr) {
            child->Render(context, state);
        }
    }
}

D2D1_RECT_F StackPanel::ContentRect(D2D1_RECT_F rect) const
{
    return math::Inset(rect, padding_.left, padding_.top, padding_.right, padding_.bottom);
}

float StackPanel::FixedMainSizeAt(size_t index) const
{
    return index < item_fixed_main_sizes_.size() ? item_fixed_main_sizes_[index] : 0.0f;
}

namespace {

constexpr float kScrollPanelThumbMinHeight = 14.0f;
constexpr float kScrollPanelThumbWidth = 1.5f;
constexpr float kScrollPanelThumbCornerRadius = 0.75f;
constexpr float kScrollPanelThumbOpacity = 0.35f;

D2D1_COLOR_F WithOpacity(D2D1_COLOR_F color, float opacity)
{
    color.a = opacity;
    return color;
}

} // namespace

ScrollPanel::ScrollPanel(UiElementMetadata metadata) : UiElement(metadata)
{
}

UiElement* ScrollPanel::SetContent(std::unique_ptr<UiElement> content)
{
    content_ = AddChild(std::move(content));
    return content_;
}

UiElement* ScrollPanel::Content()
{
    return content_;
}

const UiElement* ScrollPanel::Content() const
{
    return content_;
}

void ScrollPanel::SetScrollStep(float step)
{
    scroll_step_ = (std::max)(1.0f, step);
}

D2D1_SIZE_F ScrollPanel::Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const
{
    measured_content_ = content_ != nullptr ? content_->Measure(context, available_size) : D2D1_SIZE_F{};
    return available_size;
}

void ScrollPanel::Arrange(D2D1_RECT_F final_rect)
{
    UiElement::Arrange(final_rect);
    scroll_offset_ = std::clamp(scroll_offset_, 0.0f, MaxScrollOffset());
    if (content_ != nullptr) {
        const float content_height = ContentHeight();
        content_->Arrange(D2D1::RectF(final_rect.left, final_rect.top - EffectiveScrollOffset(), final_rect.right, final_rect.top - EffectiveScrollOffset() + content_height));
    }
}

void ScrollPanel::Render(const UiDrawContext& context, UiRootState state) const
{
    if (context.d2d_context == nullptr) {
        return;
    }

    if (content_ != nullptr) {
        const D2D1_RECT_F viewport = ViewportRect();
        context.d2d_context->PushAxisAlignedClip(viewport, D2D1_ANTIALIAS_MODE_ALIASED);
        content_->Render(context, state);
        context.d2d_context->PopAxisAlignedClip();
    }

    for (size_t index = 0; index < ChildCount(); ++index) {
        const UiElement* child = ChildAt(index);
        if (child != nullptr && child != content_) {
            child->Render(context, state);
        }
    }

    const float max_scroll = MaxScrollOffset();
    if (max_scroll <= 0.0f) {
        return;
    }

    const UiDraw draw(context);
    const D2D1_RECT_F thumb = ScrollbarThumbRect();
    draw.FillRoundedRect(
        D2D1::RoundedRect(thumb, kScrollPanelThumbCornerRadius, kScrollPanelThumbCornerRadius),
        WithOpacity(ui_theme::color::kMutedText, kScrollPanelThumbOpacity));
}

UiEventResult ScrollPanel::OnPointerEvent(const UiPointerEvent& event)
{
    if (event.type == UiEventType::PointerWheel) {
        ScrollByWheelDelta(event.wheel_delta);
        return UiEventResult{.handled = true};
    }

    if (MaxScrollOffset() <= 0.0f) {
        return {};
    }

    const D2D1_RECT_F thumb = ScrollbarThumbRect();
    if (event.type == UiEventType::PointerDown && event.button == UiPointerButton::Left && math::Contains(thumb, event.point)) {
        drag_thumb_pointer_offset_ = event.point.y - thumb.top;
        return UiEventResult{
            .handled = true,
            .capture = UiCaptureRequest::Capture,
            .focus = UiFocusRequest::ClearFocus,
        };
    }

    if (event.type == UiEventType::PointerMove && event.captured == Id()) {
        SetScrollOffsetFromThumbTop(event.point.y - drag_thumb_pointer_offset_);
        return UiEventResult{.handled = true};
    }

    if (event.type == UiEventType::PointerUp && event.captured == Id() && event.button == UiPointerButton::Left) {
        SetScrollOffsetFromThumbTop(event.point.y - drag_thumb_pointer_offset_);
        return UiEventResult{
            .handled = true,
            .capture = UiCaptureRequest::Release,
        };
    }

    return {};
}

float ScrollPanel::ContentHeight() const
{
    return (std::max)(math::RectHeight(Rect()), measured_content_.height);
}

float ScrollPanel::ViewportHeight() const
{
    return (std::max)(0.0f, math::RectHeight(ViewportRect()));
}

float ScrollPanel::MaxScrollOffset() const
{
    return (std::max)(0.0f, ContentHeight() - ViewportHeight());
}

float ScrollPanel::EffectiveScrollOffset() const
{
    return std::clamp(scroll_offset_, 0.0f, MaxScrollOffset());
}

bool ScrollPanel::ScrollByWheelDelta(int wheel_delta)
{
    const float max_scroll = MaxScrollOffset();
    if (max_scroll <= 0.0f || wheel_delta == 0) {
        scroll_offset_ = 0.0f;
        return false;
    }

    const float steps = static_cast<float>(wheel_delta) / static_cast<float>(WHEEL_DELTA);
    return SetScrollOffset(scroll_offset_ - steps * scroll_step_);
}

bool ScrollPanel::SetScrollOffset(float offset)
{
    const float old_offset = scroll_offset_;
    scroll_offset_ = std::clamp(offset, 0.0f, MaxScrollOffset());
    if (content_ != nullptr) {
        const D2D1_RECT_F viewport = Rect();
        const float content_height = ContentHeight();
        content_->Arrange(D2D1::RectF(viewport.left, viewport.top - EffectiveScrollOffset(), viewport.right, viewport.top - EffectiveScrollOffset() + content_height));
    }
    return scroll_offset_ != old_offset;
}

bool ScrollPanel::SetScrollOffsetFromThumbTop(float thumb_top)
{
    const D2D1_RECT_F track = ScrollbarTrackRect();
    const D2D1_RECT_F thumb = ScrollbarThumbRect();
    const float max_thumb_travel = (std::max)(0.0f, math::RectHeight(track) - math::RectHeight(thumb));
    if (max_thumb_travel <= 0.0f) {
        return SetScrollOffset(0.0f);
    }

    const float clamped_thumb_top = std::clamp(thumb_top, track.top, track.bottom - math::RectHeight(thumb));
    const float ratio = (clamped_thumb_top - track.top) / max_thumb_travel;
    return SetScrollOffset(ratio * MaxScrollOffset());
}

D2D1_RECT_F ScrollPanel::ViewportRect() const
{
    return Rect();
}

D2D1_RECT_F ScrollPanel::ScrollbarTrackRect() const
{
    const D2D1_RECT_F viewport = ViewportRect();
    return D2D1::RectF(
        viewport.right - ui_theme::metrics::kSmallGap,
        viewport.top,
        viewport.right - ui_theme::metrics::kSmallGap + kScrollPanelThumbWidth,
        viewport.bottom);
}

D2D1_RECT_F ScrollPanel::ScrollbarThumbRect() const
{
    const D2D1_RECT_F track = ScrollbarTrackRect();
    const float max_scroll = MaxScrollOffset();
    const float track_height = (std::max)(0.5f, math::RectHeight(track));
    const float thumb_height = (std::max)(kScrollPanelThumbMinHeight, track_height * ViewportHeight() / ContentHeight());
    const float thumb_top = max_scroll > 0.0f
        ? track.top + (track_height - thumb_height) * EffectiveScrollOffset() / max_scroll
        : track.top;
    return D2D1::RectF(track.left, thumb_top, track.right, thumb_top + thumb_height);
}
