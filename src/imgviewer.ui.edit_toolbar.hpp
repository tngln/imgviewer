#pragma once

#include <array>
#include <cstddef>

#include <d2d1_1.h>

#include "imgviewer.edit.hpp"
#include "ui.button.hpp"
#include "ui.events.hpp"
#include "ui.panel.hpp"

struct ImgViewerUiEditToolbarState final {
    bool visible = false;
    ImgViewerEditTool tool = ImgViewerEditTool::Select;
    bool dirty = false;
    bool can_undo = false;
    bool can_redo = false;
};

class ImgViewerUiEditToolbar final {
public:
    enum class ButtonKey : size_t {
        Select,
        Pen,
        Text,
        Crop,
        RotateClockwise,
        Undo,
        Redo,
        SaveAs,
        Exit,
        Count,
    };

    static constexpr size_t kButtonCount = static_cast<size_t>(ButtonKey::Count);
    static constexpr size_t ButtonIndex(ButtonKey button);

    explicit ImgViewerUiEditToolbar(UiElement& root);

    void SetScalePercent(int percent);
    void SetState(ImgViewerUiEditToolbarState state);
    D2D1_RECT_F Rect() const;
    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const;
    void Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F anchor_toolbar_rect);
    void Render(const UiDrawContext& draw_context, UiRootState state);
    UiEventResult OnPointerEvent(const UiPointerEvent& event);

private:
    struct ButtonInstance final {
        UiElementId id = UiElementId::None;
        IconButton* element = nullptr;
    };

    IconButton* Button(ButtonKey button);
    void UpdateVisualState();

    std::array<ButtonInstance, kButtonCount> buttons_{};
    StackPanel* panel_ = nullptr;
    D2D1_RECT_F toolbar_rect_ = {};
    ImgViewerUiEditToolbarState state_;
    int scale_percent_ = 125;
};
