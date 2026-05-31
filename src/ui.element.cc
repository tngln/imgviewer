#include "ui.element.hpp"

#include <utility>

UiElement::UiElement(UiElementMetadata metadata) : metadata_(metadata) {}

void UiElement::SetRect(D2D1_RECT_F rect)
{
    rect_ = rect;
}

D2D1_RECT_F UiElement::Rect() const
{
    return rect_;
}

const UiElementMetadata& UiElement::Metadata() const
{
    return metadata_;
}

UiElementId UiElement::Id() const
{
    return metadata_.id;
}

UiCommand UiElement::Command() const
{
    return metadata_.command;
}

bool UiElement::Contains(D2D1_POINT_2F point) const
{
    return point.x >= rect_.left && point.x < rect_.right && point.y >= rect_.top && point.y < rect_.bottom;
}

void UiElement::Draw(const UiDrawContext&, UiElementState) const {}

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
    for (auto child = children_.rbegin(); child != children_.rend(); ++child) {
        const UiElement* found = (*child)->HitTest(point);
        if (found != nullptr) {
            return found;
        }
    }

    return Contains(point) ? this : nullptr;
}
