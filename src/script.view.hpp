#pragma once

#include <d2d1_1.h>

#include "ui.draw.hpp"
#include "ui.events.hpp"

class PopupHost;

class ScriptView {
public:
    virtual ~ScriptView() = default;

    virtual const wchar_t* AccessibilityName() const = 0;
    virtual void Render(const UiDrawContext& context) = 0;
    virtual UiEventResult OnInputEvent(const UiInputEvent& event)
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
    virtual UiEventResult OnPointerEvent(const UiPointerEvent&) { return {}; }
    virtual UiEventResult OnKeyEvent(const UiKeyEvent&) { return {}; }
    virtual bool HandleUiAction(UiAction, PopupHost*) { return false; }
    virtual bool IsPointInCaptionDragArea(D2D1_POINT_2F) const { return false; }
};
