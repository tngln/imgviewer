#pragma once

#include <array>
#include <cstddef>

#include <d2d1_1.h>

#include "ui.button.hpp"
#include "ui.element.hpp"
#include "ui.events.hpp"
#include "ui.panel.hpp"

#include "ui.root.hpp"

class ImgViewerUiToolbar final {
public:
    enum class ButtonKey : size_t {
        PreviousImage,
        NextImage,
        CaptureRegion,
        ZoomIn,
        ZoomOut,
        FitWindow,
        ActualSize,
        RotateClockwise,
        FlipHorizontal,
        FlipVertical,
        ResetView,
        ColorPicker,
        Count,
    };

    static constexpr size_t kButtonCount = static_cast<size_t>(ButtonKey::Count);
    static constexpr size_t ButtonIndex(ButtonKey button);

    explicit ImgViewerUiToolbar(UiElement& root);

    void SetScalePercent(int percent);
    D2D1_RECT_F Rect() const;
    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const;
    void Arrange(D2D1_RECT_F final_rect);
    void Render(const UiDrawContext& draw_context, UiRootState state, bool color_picker_active);
    UiEventResult OnPointerEvent(const UiPointerEvent& event);

private:
    struct ButtonInstance final {
        UiElementId id = UiElementId::None;
        IconButton* element = nullptr;
    };

    UiEventResult OnDragHandlePointerEvent(const UiPointerEvent& event);
    void BeginDrag(D2D1_POINT_2F point);
    void Drag(D2D1_POINT_2F point);
    void EndDrag();
    void ClampToViewport(D2D1_SIZE_F viewport_size);
    IconButton* Button(ButtonKey button);
    const IconButton* Button(ButtonKey button) const;
    void RenderButton(
        ButtonKey button,
        const UiDrawContext& draw_context,
        UiRootState state,
        bool active = false,
        bool danger = false);

    std::array<ButtonInstance, kButtonCount> buttons_{};
    StackPanel* button_panel_ = nullptr;
    UiElement* drag_handle_ = nullptr;
    UiElementId drag_handle_id_ = UiElementId::None;
    D2D1_RECT_F toolbar_rect_ = D2D1_RECT_F{};
    D2D1_POINT_2F toolbar_position_ = D2D1_POINT_2F{};
    D2D1_POINT_2F drag_offset_ = D2D1_POINT_2F{};
    int scale_percent_ = 125;
    bool position_initialized_ = false;
    bool dragging_ = false;
};
