#pragma once

#include <array>
#include <cstddef>
#include <memory>

#include <d2d1_1.h>

#include "imgviewer.ui.floating_toolbar.hpp"
#include "ui.button.hpp"
#include "ui.events.hpp"

struct ImgViewerUiSelectionToolstripState final {
    bool visible = false;
};

class ImgViewerUiSelectionToolstrip final {
public:
    enum class ButtonKey : size_t {
        Copy,
        Mosaic,
        Count,
    };

    static constexpr size_t kButtonCount = static_cast<size_t>(ButtonKey::Count);
    static constexpr size_t ButtonIndex(ButtonKey button);

    explicit ImgViewerUiSelectionToolstrip(UiElement& root);

    void SetScalePercent(int percent);
    void SetState(ImgViewerUiSelectionToolstripState state);
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

    void UpdateVisualState();

    std::array<ButtonInstance, kButtonCount> buttons_{};
    std::unique_ptr<ImgViewerFloatingToolbar> toolbar_;
    ImgViewerUiSelectionToolstripState state_;
    int scale_percent_ = 125;
};
