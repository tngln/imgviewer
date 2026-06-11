#pragma once

#include <memory>

#include <d2d1_1.h>

#include "imgviewer.ui.toolstrip.hpp"
#include "ui.events.hpp"

struct ImgViewerUiSelectionToolstripState final {
    bool visible = false;
};

class ImgViewerUiSelectionToolstrip final {
public:
    explicit ImgViewerUiSelectionToolstrip(UiElement& root);

    void SetScalePercent(int percent);
    void SetState(ImgViewerUiSelectionToolstripState state);
    D2D1_RECT_F Rect() const;
    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const;
    void Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F anchor_toolbar_rect);
    void Render(const UiDrawContext& draw_context, UiRootState state);
    UiEventResult OnPointerEvent(const UiPointerEvent& event);

private:
    std::unique_ptr<ImgViewerUiToolStrip> toolstrip_;
    ImgViewerUiSelectionToolstripState state_;
};
