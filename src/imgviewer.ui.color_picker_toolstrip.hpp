#pragma once

#include <memory>
#include <string>

#include <d2d1_1.h>

#include "imgviewer.ui.toolstrip.hpp"
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
    std::unique_ptr<ImgViewerUiToolStrip> toolstrip_;
    UiElement* value_element_ = nullptr;
    ImgViewerUiColorPickerToolstripState state_;
    std::wstring display_text_;
};
