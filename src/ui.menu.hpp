#pragma once

#include <vector>

#include "ui.element.hpp"
#include "ui.events.hpp"

struct MenuItem final {
    const wchar_t* text = L"";
    ImgViewerAction action = ImgViewerAction::None;
    bool separator = false;
    bool checked = false;
    bool enabled = true;
    std::vector<MenuItem> children;
};

class MenuOverlay final {
public:
    bool IsOpen() const;
    void Open(D2D1_POINT_2F origin, std::vector<MenuItem> items);
    void Close();
    D2D1_POINT_2F Origin() const;
    D2D1_SIZE_F DesiredSize() const;
    D2D1_RECT_F Bounds() const;
    const std::vector<MenuItem>& Items() const;
    void Draw(const UiDrawContext& context, UiElementState state) const;
    UiEventResult OnInputEvent(const UiInputEvent& event);
    UiEventResult OnPointerEvent(const UiPointerEvent& event);
    UiEventResult OnKeyEvent(const UiKeyEvent& event);
    bool Contains(D2D1_POINT_2F point) const;

private:
    size_t ItemAt(D2D1_POINT_2F point) const;
    D2D1_RECT_F ItemRect(size_t index) const;
    void MoveSelection(int delta);

    D2D1_POINT_2F origin_ = {};
    std::vector<MenuItem> items_;
    bool open_ = false;
    size_t selected_ = 0;
};
