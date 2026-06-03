#pragma once

#include <memory>

#include <d2d1_1.h>
#include <dwrite.h>

#include "ui.element.hpp"
#include "ui.events.hpp"

struct ImgViewerUiState final {
    UiElementId hovered = UiElementId::None;
    UiElementId pressed = UiElementId::None;
};

#include "imgviewer.ui.titlebar.hpp"
#include "imgviewer.ui.toast.hpp"
#include "imgviewer.ui.toolbar.hpp"
#include "ui.menu.hpp"

class ImgViewerUi final {
public:
    ImgViewerUi();

    UiElement* Root();
    const UiElement* Root() const;

    void Draw(
        ID2D1DeviceContext* d2d_context,
        D2D1_SIZE_F viewport_size,
        IDWriteFactory* dwrite_factory,
        IDWriteTextFormat* body_text_format,
        IDWriteTextFormat* icon_text_format,
        ImgViewerUiState state);
    UiEventResult OnPointerEvent(const UiPointerEvent& event);
    UiEventResult OnKeyEvent(const UiKeyEvent& event);
    bool HandleUiAction(ImgViewerAction action);
    bool IsPointInCaptionDragArea(D2D1_POINT_2F point) const;
    void SetTitleText(const wchar_t* title);
    void ShowToast(const wchar_t* text);
    bool HideToast();
    void SetWindowState(bool top_most, bool maximized);
    void SetColorPickerActive(bool active);

private:
    void Layout(D2D1_SIZE_F viewport_size);

    std::unique_ptr<UiElement> root_;
    UiElementIdGenerator ids_;
    ImgViewerUiTitleBar titlebar_;
    ImgViewerUiToolbar toolbar_;
    ImgViewerUiToast toast_;
    bool top_most_ = false;
    bool maximized_ = false;
    bool color_picker_active_ = false;
    MenuOverlay menu_;
};
