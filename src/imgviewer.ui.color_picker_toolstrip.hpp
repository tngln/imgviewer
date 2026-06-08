#pragma once

#include <memory>
#include <string>

#include <d2d1_1.h>

#include "imgviewer.ui.floating_toolbar.hpp"
#include "ui.button.hpp"
#include "ui.events.hpp"

struct ImgViewerUiColorPickerToolstripState final {
    bool visible = false;
    bool has_sample = false;
    std::wstring hex_text;
};

class ImgViewerUiColorPickerToolstrip final {
public:
    explicit ImgViewerUiColorPickerToolstrip(UiElement& root);

    void SetScalePercent(int percent);
    void SetState(ImgViewerUiColorPickerToolstripState state);
    D2D1_RECT_F Rect() const;
    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const;
    void Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F anchor_toolbar_rect);
    void Render(const UiDrawContext& draw_context, UiRootState state);
    UiEventResult OnPointerEvent(const UiPointerEvent& event);
    bool IsValueElement(UiElementId id) const;
    const wchar_t* ValueText() const;

private:
    void UpdateVisualState();

    std::unique_ptr<ImgViewerFloatingToolbar> toolbar_;
    UiElement* value_element_ = nullptr;
    IconButton* copy_button_ = nullptr;
    ImgViewerUiColorPickerToolstripState state_;
    std::wstring display_text_;
    int scale_percent_ = 125;
};
