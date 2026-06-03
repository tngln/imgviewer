#pragma once

#include <d2d1_1.h>
#include <dwrite.h>
#include <cstddef>
#include <memory>

#include "ui.element.hpp"
#include "ui.events.hpp"
#include "ui.a11y.hpp"

class ImgViewerUi;

class UiController final : public UiAccessibilitySource {
public:
    UiController();
    ~UiController();

    UiEventResult OnPointerMove(D2D1_POINT_2F point);
    UiEventResult OnPointerDown(D2D1_POINT_2F point);
    UiEventResult OnPointerUp(D2D1_POINT_2F point);
    UiEventResult OnPointerLeave();
    UiEventResult OnPointerEvent(const UiPointerEvent& event);
    UiEventResult OnKeyEvent(const UiKeyEvent& event);

    UiElementId HoveredElement() const;
    UiElementId PressedElement() const;
    UiElementId FocusedElement() const;
    UiElementId CapturedElement() const;

    void Draw(
        ID2D1DeviceContext* d2d_context,
        D2D1_SIZE_F viewport_size,
        IDWriteFactory* dwrite_factory,
        IDWriteTextFormat* body_text_format,
        IDWriteTextFormat* icon_text_format);
    const wchar_t* AccessibilityRootName() const override;
    size_t ElementCount() const override;
    const UiElementMetadata* ElementMetadataAt(size_t index) const override;
    const UiElementMetadata* ElementMetadata(UiElementId id) const override;
    D2D1_RECT_F ElementRect(UiElementId id) const override;
    bool IsElementEnabled(UiElementId id) const override;
    bool IsPointInCaptionDragArea(D2D1_POINT_2F point) const;
    void SetTitleText(const wchar_t* title);
    void SetActionEnabled(ImgViewerAction action, bool enabled);
    void SetWindowState(bool top_most, bool maximized);

private:
    UiElementId HitTest(D2D1_POINT_2F point) const;
    const UiElementMetadata* MetadataForElement(UiElementId id) const;
    UiEventResult DispatchPointerEvent(const UiPointerEvent& event);
    UiEventResult DispatchKeyEvent(const UiKeyEvent& event);
    void ApplyEventResult(const UiEventResult& result, UiElementId target);
    void SetActionEnabledRecursive(UiElement* element, ImgViewerAction action, bool enabled);

    std::unique_ptr<ImgViewerUi> imgviewer_ui_;
    UiElementId hovered_id_ = UiElementId::None;
    UiElementId pressed_id_ = UiElementId::None;
    UiElementId focused_id_ = UiElementId::None;
    UiElementId captured_id_ = UiElementId::None;
};
