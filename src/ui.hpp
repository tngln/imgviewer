#pragma once

#include <d2d1_1.h>
#include <dwrite.h>
#include <cstddef>
#include <memory>

#include "ui.element.hpp"
#include "ui.events.hpp"
#include "ui.a11y.hpp"
#include "ui.root.hpp"

class UiController final : public UiAccessibilitySource {
public:
    explicit UiController(std::unique_ptr<UiRoot> root);
    ~UiController();

    void ResetRoot(std::unique_ptr<UiRoot> root);
    UiRoot* Root();
    const UiRoot* Root() const;
    UiEventResult OnInputEvent(const UiInputEvent& event);
    UiEventResult OnPointerEvent(const UiPointerEvent& event);
    UiEventResult OnKeyEvent(const UiKeyEvent& event);

    UiElementId HoveredElement() const;
    UiElementId PressedElement() const;
    UiElementId FocusedElement() const;
    UiElementId CapturedElement() const;

    void Draw(const UiDrawContext& context);
    const wchar_t* AccessibilityRootName() const override;
    size_t ElementCount() const override;
    const UiElementMetadata* ElementMetadataAt(size_t index) const override;
    const UiElementMetadata* ElementMetadata(UiElementId id) const override;
    D2D1_RECT_F ElementRect(UiElementId id) const override;
    bool IsElementEnabled(UiElementId id) const override;
    const wchar_t* ElementValue(UiElementId id) const override;
    double ElementRangeValue(UiElementId id) const override;
    double ElementRangeMinimum(UiElementId id) const override;
    double ElementRangeMaximum(UiElementId id) const override;
    double ElementRangeSmallChange(UiElementId id) const override;
    double ElementRangeLargeChange(UiElementId id) const override;
    HRESULT SetElementRangeValue(UiElementId id, double value) override;
    bool IsPointInCaptionDragArea(D2D1_POINT_2F point) const;
    void SetTitleText(const wchar_t* title);
    void ShowToast(const wchar_t* text);
    bool HideToast();
    void SetActionEnabled(UiAction action, bool enabled);
    void SetWindowState(bool top_most, bool maximized);
    void SetColorPickerActive(bool active);

private:
    UiElementId HitTest(D2D1_POINT_2F point) const;
    const UiElementMetadata* MetadataForElement(UiElementId id) const;
    UiEventResult DispatchPointerEvent(const UiPointerEvent& event);
    UiEventResult DispatchKeyEvent(const UiKeyEvent& event);
    void ApplyEventResult(const UiEventResult& result, UiElementId target);
    void SetActionEnabledRecursive(UiElement* element, UiAction action, bool enabled);

    std::unique_ptr<UiRoot> root_;
    UiElementId hovered_id_ = UiElementId::None;
    UiElementId pressed_id_ = UiElementId::None;
    UiElementId focused_id_ = UiElementId::None;
    UiElementId captured_id_ = UiElementId::None;
};
