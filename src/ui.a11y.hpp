#pragma once

#include <windows.h>

#include <ole2.h>
#include <UIAutomationCore.h>
#include <UIAutomationCoreApi.h>

#include <cstddef>

#include <d2d1.h>

#include "ui.element.hpp"

class UiAccessibilitySource {
public:
    virtual ~UiAccessibilitySource() = default;

    virtual const wchar_t* AccessibilityRootName() const = 0;
    virtual size_t ElementCount() const = 0;
    virtual const UiElementMetadata* ElementMetadataAt(size_t index) const = 0;
    virtual const UiElementMetadata* ElementMetadata(UiElementId id) const = 0;
    virtual D2D1_RECT_F ElementRect(UiElementId id) const = 0;
    virtual bool IsElementEnabled(UiElementId id) const = 0;
};

HRESULT CreateUiAccessibilityProvider(
    HWND hwnd,
    UiAccessibilitySource* ui,
    IRawElementProviderSimple** provider);
