#pragma once

#include <functional>
#include <string>
#include <vector>

#include "ui.element.hpp"
#include "ui.events.hpp"

struct DropdownOption final {
    const wchar_t* text = L"";
    UiAction action = kUiActionNone;
};

class Checkbox final : public UiElement {
public:
    Checkbox(UiElementMetadata metadata, const wchar_t* text, bool checked);

    bool IsChecked() const;
    void SetChecked(bool checked);
    void SetOnToggled(std::function<void(bool)> handler);
    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const override;
    void Render(const UiDrawContext& context, UiRootState state) const override;
    UiEventResult OnPointerEvent(const UiPointerEvent& event) override;
    UiEventResult OnKeyEvent(const UiKeyEvent& event) override;

private:
    const wchar_t* text_ = L"";
    bool checked_ = false;
    std::function<void(bool)> toggled_handler_;
};

class RadioButton final : public UiElement {
public:
    RadioButton(UiElementMetadata metadata, const wchar_t* text, bool selected);

    bool IsSelected() const;
    void SetSelected(bool selected);
    void SetOnSelected(std::function<void()> handler);
    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const override;
    void Render(const UiDrawContext& context, UiRootState state) const override;
    UiEventResult OnPointerEvent(const UiPointerEvent& event) override;
    UiEventResult OnKeyEvent(const UiKeyEvent& event) override;

private:
    const wchar_t* text_ = L"";
    bool selected_ = false;
    std::function<void()> selected_handler_;
};

class Dropdown final : public UiElement {
public:
    Dropdown(UiElementMetadata metadata, std::vector<DropdownOption> options);

    size_t SelectedIndex() const;
    UiAction SelectedAction() const;
    void SetSelectedIndex(size_t index);
    void SetOnSelectionChanged(std::function<void(size_t)> handler);
    void SetOptions(std::vector<DropdownOption> options);
    bool IsExpanded() const;
    void Collapse();
    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const override;
    void Render(const UiDrawContext& context, UiRootState state) const override;
    UiEventResult OnPointerEvent(const UiPointerEvent& event) override;
    UiEventResult OnKeyEvent(const UiKeyEvent& event) override;

private:
    HRESULT OpenPopup(PopupHost* popup_host);
    size_t OptionAt(D2D1_POINT_2F point) const;
    D2D1_RECT_F OptionRect(size_t index) const;

    std::vector<DropdownOption> options_;
    size_t selected_index_ = 0;
    size_t hovered_index_ = 0;
    bool expanded_ = false;
    std::function<void(size_t)> selection_changed_handler_;
};
