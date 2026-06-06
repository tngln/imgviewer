#pragma once

#include <d2d1_1.h>
#include <dwrite.h>

#include "ui.draw.hpp"
#include "ui.element.hpp"
#include "ui.events.hpp"

struct UiRootState final {
    UiElementId hovered = UiElementId::None;
    UiElementId pressed = UiElementId::None;
    UiElementId focused = UiElementId::None;
};

class UiRoot {
public:
    virtual ~UiRoot() = default;

    virtual UiElement* Root() = 0;
    virtual const UiElement* Root() const = 0;
    virtual const wchar_t* AccessibilityRootName() const = 0;
    virtual void Draw(const UiDrawContext& context, UiRootState state) = 0;
    virtual UiEventResult OnInputEvent(const UiInputEvent&) { return {}; }
    virtual UiEventResult OnPointerEvent(const UiPointerEvent&) { return {}; }
    virtual UiEventResult OnKeyEvent(const UiKeyEvent&) { return {}; }
    virtual bool HandleUiAction(UiAction) { return false; }
    virtual void ApplyElementEffect(UiElementId) {}
    virtual UiElementId HitTest(D2D1_POINT_2F point) const
    {
        const UiElement* element = Root()->HitTest(point);
        return element != nullptr ? element->Id() : UiElementId::None;
    }
    virtual bool IsPointInCaptionDragArea(D2D1_POINT_2F) const { return false; }
    virtual void SetTitleText(const wchar_t*) {}
    virtual void ShowToast(const wchar_t*) {}
    virtual bool HideToast() { return false; }
    virtual void SetWindowState(bool, bool) {}
    virtual void SetColorPickerActive(bool) {}
    virtual void SetToolbarScalePercent(int) {}
    virtual void SetActionEnabled(UiAction, bool) {}
    virtual const wchar_t* ElementValue(UiElementId) const { return L""; }
    virtual double ElementRangeValue(UiElementId) const { return 0.0; }
    virtual double ElementRangeMinimum(UiElementId) const { return 0.0; }
    virtual double ElementRangeMaximum(UiElementId) const { return 0.0; }
    virtual double ElementRangeSmallChange(UiElementId) const { return 1.0; }
    virtual double ElementRangeLargeChange(UiElementId) const { return 10.0; }
    virtual HRESULT SetElementRangeValue(UiElementId, double) { return E_NOTIMPL; }
};
