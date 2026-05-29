#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <ole2.h>
#include <UIAutomationCore.h>
#include <UIAutomationCoreApi.h>

class Renderer;

HRESULT CreateUiAccessibilityProvider(
    HWND hwnd,
    Renderer* renderer,
    IRawElementProviderSimple** provider);
