#include "ui.button_behavior.hpp"

#include <windows.h>

#include "ui.action.hpp"

UiEventResult ToolButtonPointerEvent(UiElement& button, const UiPointerEvent& event)
{
    if (event.button != UiPointerButton::Left &&
        (event.type == UiEventType::PointerDown || event.type == UiEventType::PointerUp)) {
        return {};
    }

    if (event.type == UiEventType::PointerDown) {
        const bool can_activate = button.IsEnabled();
        return UiEventResult{
            .handled = true,
            .capture = can_activate ? UiCaptureRequest::Capture : UiCaptureRequest::None,
            .focus = can_activate && button.IsFocusable() ? UiFocusRequest::FocusTarget : UiFocusRequest::None,
            .focus_target = can_activate ? button.Id() : UiElementId::None,
        };
    }

    if (event.type == UiEventType::PointerUp && event.captured == button.Id()) {
        const bool activated = button.IsEnabled() && event.target == button.Id();
        if (activated && button.HasOnClick()) {
            button.InvokeClick();
            return UiEventResult{
                .handled = true,
                .capture = UiCaptureRequest::Release,
            };
        }
        return UiEventResult{
            .handled = true,
            .capture = UiCaptureRequest::Release,
            .action = activated ? button.Action() : kUiActionNone,
        };
    }

    return {};
}

UiEventResult ToolButtonKeyEvent(UiElement& button, const UiKeyEvent& event)
{
    if (event.type != UiEventType::KeyDown || !button.IsEnabled()) {
        return {};
    }

    if (event.virtual_key != VK_RETURN && event.virtual_key != VK_SPACE) {
        return {};
    }

    if (button.HasOnClick()) {
        button.InvokeClick();
        return UiEventResult{
            .handled = true,
        };
    }

    return UiEventResult{
        .handled = true,
        .action = button.Action(),
    };
}
