#pragma once

#include <windows.h>

#include <wil/com.h>

#include "ui.a11y.hpp"
#include "win32.window.hpp"

HRESULT ResetUiAccessibilityProvider(
    HWND hwnd,
    UINT action_message,
    UiAccessibilitySource* ui,
    wil::com_ptr<IRawElementProviderSimple>* provider);

win32::WindowMessageResult HandleUiAccessibilityGetObjectMessage(
    HWND hwnd,
    WPARAM wparam,
    LPARAM lparam,
    IRawElementProviderSimple* provider);
