#pragma once

#include <string>

#include <d2d1_1.h>

#include "ui.draw.hpp"
#include "ui.element.hpp"

struct ImgViewerUiInfoPanelState final {
    bool visible = false;
    std::wstring name;
    std::wstring path;
    std::wstring dimensions;
    std::wstring type;
    std::wstring file_size;
    std::wstring modified_time;
    std::wstring sequence;
    std::wstring zoom;
    std::wstring rotation;
    std::wstring flips;
};

class ImgViewerUiInfoPanel final {
public:
    ImgViewerUiInfoPanel(UiElement& root, UiElementIdGenerator& ids);

    void SetState(ImgViewerUiInfoPanelState state);
    bool IsVisible() const;
    void Draw(const UiDrawContext& draw_context) const;
    UiEventResult OnPointerEvent(const UiPointerEvent& event) const;

private:
    void Layout(D2D1_SIZE_F viewport_size) const;
    void DrawRow(const UiDraw& draw, const wchar_t* label, const std::wstring& value, float top) const;

    UiElement* panel_ = nullptr;
    UiElementId panel_id_ = UiElementId::None;
    ImgViewerUiInfoPanelState state_;
};
