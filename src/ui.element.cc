#include "ui.element.hpp"

#include <utility>

#include "ui.events.hpp"

UiElementId UiElementIdGenerator::Next()
{
    return static_cast<UiElementId>(next_id_++);
}

namespace {

UiElementIdGenerator& GlobalUiElementIdGenerator()
{
    static UiElementIdGenerator generator;
    return generator;
}

UiElementMetadata MakeUiElementMetadata(
    UiElementId id,
    UiElementRole role,
    UiAction action,
    const wchar_t* name,
    const wchar_t* tooltip,
    const wchar_t* automation_id,
    bool is_control,
    bool is_content)
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

UiElementMetadata UiMetadata(
    UiElementRole role,
    UiAction action,
    const wchar_t* name,
    const wchar_t* tooltip,
    const wchar_t* automation_id,
    bool is_control,
    bool is_content)
{
    return MakeUiElementMetadata(
        GlobalUiElementIdGenerator().Next(),
        role,
        action,
        name,
        tooltip,
        automation_id,
        is_control,
        is_content);
}

UiElementMetadata UiRootMetadata(
    UiElementRole role,
    UiAction action,
    const wchar_t* name,
    const wchar_t* tooltip,
    const wchar_t* automation_id,
    bool is_control,
    bool is_content)
{
    return MakeUiElementMetadata(
        UiElementId::None,
        role,
        action,
        name,
        tooltip,
        automation_id,
        is_control,
        is_content);
}

UiElement::UiElement(UiElementMetadata metadata) : metadata_(metadata) {}

D2D1_RECT_F UiElement::Rect() const
{
    return rect_;
}

D2D1_SIZE_F UiElement::Measure(const UiDrawContext&, D2D1_SIZE_F) const
{
    return D2D1_SIZE_F{};
}

void UiElement::Arrange(D2D1_RECT_F final_rect)
{
    rect_ = final_rect;
}

const UiElementMetadata& UiElement::Metadata() const
{
    return metadata_;
}

UiElementId UiElement::Id() const
{
    return metadata_.id;
}

UiAction UiElement::Action() const
{
    return metadata_.action;
}

void UiElement::SetEnabled(bool enabled)
{
    enabled_ = enabled;
}

bool UiElement::IsEnabled() const
{
    return enabled_;
}

void UiElement::SetFocusable(bool focusable)
{
    focusable_ = focusable;
}

bool UiElement::IsFocusable() const
{
    return focusable_;
}

void UiElement::SetHitTestVisible(bool hit_test_visible)
{
    hit_test_visible_ = hit_test_visible;
}

bool UiElement::IsHitTestVisible() const
{
    return hit_test_visible_;
}

void UiElement::SetVisualActive(bool active)
{
    visual_active_ = active;
}

bool UiElement::IsVisualActive() const
{
    return visual_active_;
}

void UiElement::SetVisualDanger(bool danger)
{
    visual_danger_ = danger;
}

bool UiElement::IsVisualDanger() const
{
    return visual_danger_;
}

UiElementState UiElement::VisualState(UiRootState state) const
{
    return UiElementState{
        .hovered = state.hovered == Id(),
        .pressed = state.pressed == Id(),
        .active = visual_active_ || state.focused == Id(),
        .danger = visual_danger_,
        .enabled = IsEnabled(),
    };
}

bool UiElement::Contains(D2D1_POINT_2F point) const
{
    return point.x >= rect_.left && point.x < rect_.right && point.y >= rect_.top && point.y < rect_.bottom;
}

void UiElement::Render(const UiDrawContext&, UiRootState) const {}

UiEventResult UiElement::OnInputEvent(const UiInputEvent& event)
{
    switch (event.type) {
    case UiEventType::PointerMove:
    case UiEventType::PointerDown:
    case UiEventType::PointerUp:
    case UiEventType::PointerLeave:
    case UiEventType::PointerWheel:
        return OnPointerEvent(event.pointer);
    case UiEventType::KeyDown:
    case UiEventType::KeyUp:
        return OnKeyEvent(event.key);
    default:
        return {};
    }
}

UiEventResult UiElement::OnPointerEvent(const UiPointerEvent&)
{
    return {};
}

UiEventResult UiElement::OnKeyEvent(const UiKeyEvent&)
{
    return {};
}

UiElement* UiElement::AddChild(std::unique_ptr<UiElement> child)
{
    UiElement* raw_child = child.get();
    children_.push_back(std::move(child));
    return raw_child;
}

size_t UiElement::ChildCount() const
{
    return children_.size();
}

UiElement* UiElement::ChildAt(size_t index)
{
    return const_cast<UiElement*>(std::as_const(*this).ChildAt(index));
}

const UiElement* UiElement::ChildAt(size_t index) const
{
    return index < children_.size() ? children_[index].get() : nullptr;
}

UiElement* UiElement::FindById(UiElementId id)
{
    return const_cast<UiElement*>(std::as_const(*this).FindById(id));
}

const UiElement* UiElement::FindById(UiElementId id) const
{
    if (metadata_.id == id) {
        return this;
    }

    for (const auto& child : children_) {
        const UiElement* found = child->FindById(id);
        if (found != nullptr) {
            return found;
        }
    }

    return nullptr;
}

UiElement* UiElement::HitTest(D2D1_POINT_2F point)
{
    return const_cast<UiElement*>(std::as_const(*this).HitTest(point));
}

const UiElement* UiElement::HitTest(D2D1_POINT_2F point) const
{
    if (!hit_test_visible_) {
        return nullptr;
    }

    if (!Contains(point)) {
        return nullptr;
    }

    for (auto child = children_.rbegin(); child != children_.rend(); ++child) {
        const UiElement* found = (*child)->HitTest(point);
        if (found != nullptr) {
            return found;
        }
    }

    return this;
}
