#pragma once

#include <memory>
#include <string>

#include <d2d1_1.h>
#include <dwrite.h>

#include "ui.button.hpp"
#include "ui.events.hpp"

struct ImageViewerUiState final {
    UiElementId hovered = UiElementId::None;
    UiElementId pressed = UiElementId::None;
};

class ImageViewerUi final {
public:
    ImageViewerUi();

    UiElement* Root();
    const UiElement* Root() const;

    void Draw(
        ID2D1DeviceContext* d2d_context,
        D2D1_SIZE_F viewport_size,
        IDWriteFactory* dwrite_factory,
        IDWriteTextFormat* body_text_format,
        IDWriteTextFormat* icon_text_format,
        ImageViewerUiState state);
    UiEventResult OnPointerEvent(const UiPointerEvent& event);
    bool IsPointInCaptionDragArea(D2D1_POINT_2F point) const;
    void SetTitleText(const wchar_t* title);
    void SetWindowState(bool top_most, bool maximized);

private:
    void Layout(D2D1_SIZE_F viewport_size, IDWriteFactory* dwrite_factory, IDWriteTextFormat* body_text_format);
    UiEventResult OnToolbarDragHandlePointerEvent(const UiPointerEvent& event);
    void BeginToolbarDrag(D2D1_POINT_2F point);
    void DragToolbar(D2D1_POINT_2F point);
    void EndToolbarDrag();
    void ClampToolbarToViewport(D2D1_SIZE_F viewport_size);
    UiElementState ButtonState(UiElementId id, ImageViewerUiState state, bool active = false, bool danger = false) const;

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
    bool toolbar_position_initialized_ = false;
    bool toolbar_dragging_ = false;
    bool top_most_ = false;
    bool maximized_ = false;
};
