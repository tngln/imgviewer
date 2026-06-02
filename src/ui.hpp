#pragma once

#include <d2d1_1.h>
#include <dwrite.h>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "ui.button.hpp"
#include "ui.events.hpp"

class UiController final {
public:
    UiController();

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
    size_t ElementCount() const;
    const UiElementMetadata* ElementMetadataAt(size_t index) const;
    const UiElementMetadata* ElementMetadata(UiElementId id) const;
    D2D1_RECT_F ElementRect(UiElementId id) const;
    bool IsElementEnabled(UiElementId id) const;
    bool IsPointInCaptionDragArea(D2D1_POINT_2F point) const;
    void SetTitleText(const wchar_t* title);
    void SetActionEnabled(AppAction action, bool enabled);
    void SetWindowState(bool top_most, bool maximized);

private:
    void Layout(D2D1_SIZE_F viewport_size, IDWriteFactory* dwrite_factory, IDWriteTextFormat* body_text_format);
    UiElementId HitTest(D2D1_POINT_2F point) const;
    bool HitTestToolbar(D2D1_POINT_2F point) const;
    void ClampToolbarToViewport(D2D1_SIZE_F viewport_size);
    AppAction ActionFor(UiElementId id) const;
    bool IsActionEnabled(AppAction action) const;
    UiElementState ButtonState(UiElementId id, bool active = false, bool danger = false) const;
    const UiElementMetadata* MetadataForElement(UiElementId id) const;
    UiEventResult DispatchPointerEvent(const UiPointerEvent& event);
    UiEventResult DispatchKeyEvent(const UiKeyEvent& event);
    void ApplyEventResult(const UiEventResult& result, UiElementId target);
    void SetActionEnabledRecursive(UiElement* element, AppAction action, bool enabled);

    std::wstring title_text_ = L"ImgViewer";
    D2D1_RECT_F titlebar_rect_ = D2D1_RECT_F{0.0f, 0.0f, 960.0f, 48.0f};
    D2D1_RECT_F title_text_rect_ = D2D1_RECT_F{16.0f, 0.0f, 720.0f, 48.0f};
    D2D1_RECT_F toolbar_rect_ = D2D1_RECT_F{};
    D2D1_POINT_2F toolbar_position_ = D2D1_POINT_2F{};
    D2D1_POINT_2F toolbar_drag_offset_ = D2D1_POINT_2F{};
    std::unique_ptr<UiElement> root_;
    IconButton* top_most_button_ = nullptr;
    IconButton* minimize_button_ = nullptr;
    IconButton* maximize_button_ = nullptr;
    IconButton* close_button_ = nullptr;
    IconButton* previous_button_ = nullptr;
    IconButton* next_button_ = nullptr;
    IconButton* zoom_in_button_ = nullptr;
    IconButton* zoom_out_button_ = nullptr;
    IconButton* rotate_button_ = nullptr;
    IconButton* flip_horizontal_button_ = nullptr;
    IconButton* flip_vertical_button_ = nullptr;
    IconButton* reset_button_ = nullptr;
    UiElement* toolbar_drag_handle_ = nullptr;
    UiElementId hovered_id_ = UiElementId::None;
    UiElementId pressed_id_ = UiElementId::None;
    UiElementId focused_id_ = UiElementId::None;
    UiElementId captured_id_ = UiElementId::None;
    std::vector<AppAction> disabled_actions_;
    bool toolbar_position_initialized_ = false;
    bool toolbar_dragging_ = false;
    bool top_most_ = false;
    bool maximized_ = false;
};
