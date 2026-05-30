#pragma once

#include <windows.h>

#include <ole2.h>
#include <UIAutomationCore.h>
#include <UIAutomationCoreApi.h>

class UiController;

HRESULT CreateUiAccessibilityProvider(
    HWND hwnd,
    UiController* ui,
    IRawElementProviderSimple** provider);
