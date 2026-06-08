#pragma once

#include <vector>

#include "ui.element.hpp"
#include "ui.events.hpp"

struct MenuItem final {
    const wchar_t* text = L"";
    UiAction action = kUiActionNone;
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
    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const;
    D2D1_RECT_F Bounds() const;
    const std::vector<MenuItem>& Items() const;
    void UpdatePreferredWidth(const UiDrawContext& context) const;
    float PreferredWidth() const;
    void Render(const UiDrawContext& context, UiRootState state) const;
    UiEventResult OnInputEvent(const UiInputEvent& event);
    UiEventResult OnPointerEvent(const UiPointerEvent& event);
    UiEventResult OnKeyEvent(const UiKeyEvent& event);
    bool Contains(D2D1_POINT_2F point) const;

private:
    struct Panel final {
        const std::vector<MenuItem>* items = nullptr;
        D2D1_POINT_2F origin = {};
        float width = 0.0f;
    };

    D2D1_SIZE_F MeasuredSize() const;
    std::vector<Panel> Panels() const;
    const std::vector<MenuItem>* ItemsAtDepth(size_t depth) const;
    const MenuItem* SelectedItem(size_t depth) const;
    size_t PanelAt(D2D1_POINT_2F point) const;
    size_t ItemAt(size_t panel, D2D1_POINT_2F point) const;
    D2D1_RECT_F PanelRect(size_t panel) const;
    D2D1_RECT_F ItemRect(size_t panel, size_t index) const;
    void OpenChild(size_t depth);
    void TrimToDepth(size_t depth);
    void MoveSelection(int delta);

    D2D1_POINT_2F origin_ = {};
    std::vector<MenuItem> items_;
    mutable std::vector<float> preferred_widths_;
    bool open_ = false;
    std::vector<size_t> selected_path_;
};
