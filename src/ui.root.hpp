#pragma once

#include <d2d1_1.h>
#include <dwrite.h>

#include "ui.element.hpp"
#include "ui.events.hpp"

struct UiRootState final {
    UiElementId hovered = UiElementId::None;
    UiElementId pressed = UiElementId::None;
};

class UiRoot {
public:
    virtual ~UiRoot() = default;

    virtual UiElement* Root() = 0;
    virtual const UiElement* Root() const = 0;
    virtual const wchar_t* AccessibilityRootName() const = 0;
    virtual void Draw(
        ID2D1DeviceContext* d2d_context,
        D2D1_SIZE_F viewport_size,
        IDWriteFactory* dwrite_factory,
        IDWriteTextFormat* body_text_format,
        IDWriteTextFormat* icon_text_format,
        UiRootState state) = 0;
    virtual UiEventResult OnPointerEvent(const UiPointerEvent& event) = 0;
    virtual UiEventResult OnKeyEvent(const UiKeyEvent& event) = 0;
    virtual bool HandleUiAction(UiAction action) = 0;
    virtual bool IsPointInCaptionDragArea(D2D1_POINT_2F point) const = 0;
    virtual void SetTitleText(const wchar_t* title) = 0;
    virtual void ShowToast(const wchar_t* text) = 0;
    virtual bool HideToast() = 0;
    virtual void SetWindowState(bool top_most, bool maximized) = 0;
    virtual void SetColorPickerActive(bool active) = 0;
};
