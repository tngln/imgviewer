#include "ui.element.hpp"

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
