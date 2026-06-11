#pragma once

#include <memory>

#include <d2d1_1.h>

#include "imgviewer.edit.hpp"
#include "imgviewer.ui.toolstrip.hpp"
#include "ui.events.hpp"

struct ImgViewerUiEditToolbarState final {
    bool visible = false;
    ImgViewerEditTool tool = ImgViewerEditTool::Select;
    bool dirty = false;
    bool can_undo = false;
    bool can_redo = false;
};

class ImgViewerUiEditToolbar final {
public:
    explicit ImgViewerUiEditToolbar(UiElement& root);

    void SetScalePercent(int percent);
    void SetState(ImgViewerUiEditToolbarState state);
    D2D1_RECT_F Rect() const;
    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const;
    void Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F anchor_toolbar_rect);
    void Render(const UiDrawContext& draw_context, UiRootState state);
    UiEventResult OnPointerEvent(const UiPointerEvent& event);

private:
    static constexpr size_t kSelect = 0;
    static constexpr size_t kPixelSelect = 1;
    static constexpr size_t kPen = 2;
    static constexpr size_t kShape = 3;
    static constexpr size_t kText = 4;
    static constexpr size_t kCrop = 5;
    static constexpr size_t kUndo = 7;
    static constexpr size_t kRedo = 8;

    std::unique_ptr<ImgViewerUiToolStrip> toolstrip_;
    ImgViewerUiEditToolbarState state_;
};
