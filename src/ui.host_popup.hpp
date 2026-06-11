#pragma once

#include "ui.events.hpp"
#include "ui.popup.hpp"

void ClosePopupIfOpen(PopupHost* popup);
bool DispatchInputEventToPopup(PopupHost* popup, UiInputEvent event, UiEventResult* result);
bool DispatchOwnerDeactivatedToPopup(PopupHost* popup, HWND owner, UiEventResult* result);
