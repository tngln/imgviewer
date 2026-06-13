#pragma once

#include <memory>

#include <d2d1_1.h>

#include "imgviewer.ui.toolstrip.hpp"
#include "ui.events.hpp"

struct ImgViewerUiPenToolstripState final {
    bool visible = false;
    D2D1_COLOR_F color = D2D1::ColorF(D2D1::ColorF::Red);
    float width = 4.0f;
};

class ImgViewerUiPenToolstrip final {
public:
    explicit ImgViewerUiPenToolstrip(UiElement& root);

    void SetScalePercent(int percent);
    void SetState(ImgViewerUiPenToolstripState state);
    D2D1_RECT_F Rect() const;
    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const;
    void Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F anchor_toolbar_rect);
    void Render(const UiDrawContext& draw_context, UiRootState state);
    UiEventResult OnPointerEvent(const UiPointerEvent& event);

private:
    std::unique_ptr<ImgViewerUiToolStrip> toolstrip_;
    ImgViewerUiPenToolstripState state_;
};
