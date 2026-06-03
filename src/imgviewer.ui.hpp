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
#include "imgviewer.ui.toolbar.hpp"

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
    bool IsPointInCaptionDragArea(D2D1_POINT_2F point) const;
    void SetTitleText(const wchar_t* title);
    void SetWindowState(bool top_most, bool maximized);

private:
    void Layout(D2D1_SIZE_F viewport_size);

    std::unique_ptr<UiElement> root_;
    UiElementIdGenerator ids_;
    ImgViewerUiTitleBar titlebar_;
    ImgViewerUiToolbar toolbar_;
    bool top_most_ = false;
    bool maximized_ = false;
};
