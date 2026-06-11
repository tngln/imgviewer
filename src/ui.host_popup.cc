#include "ui.host_popup.hpp"

namespace {

UiInputEvent AttachPopupHost(PopupHost* popup, UiInputEvent event)
{
    event.popup_host = popup;
    if (event.type == UiEventType::KeyDown || event.type == UiEventType::KeyUp) {
        event.key.popup_host = popup;
    } else if (
        event.type == UiEventType::PointerMove ||
        event.type == UiEventType::PointerDown ||
        event.type == UiEventType::PointerUp ||
        event.type == UiEventType::PointerLeave ||
        event.type == UiEventType::PointerWheel) {
        event.pointer.popup_host = popup;
    }
    return event;
}

} // namespace

void ClosePopupIfOpen(PopupHost* popup)
{
    if (popup != nullptr && popup->IsOpen()) {
        popup->Close();
    }
}

bool DispatchInputEventToPopup(PopupHost* popup, UiInputEvent event, UiEventResult* result)
{
    if (popup == nullptr || !popup->IsOpen() || result == nullptr) {
        return false;
    }

    *result = popup->OnInputEvent(AttachPopupHost(popup, event));
    return true;
}

bool DispatchOwnerDeactivatedToPopup(PopupHost* popup, HWND owner, UiEventResult* result)
{
    return DispatchInputEventToPopup(
        popup,
        UiInputEvent{
            .type = UiEventType::OwnerDeactivated,
            .hwnd = owner,
        },
        result);
}
