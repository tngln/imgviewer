#pragma once

#include <memory>
#include <utility>

#include "ui.action.hpp"
#include "ui.button.hpp"
#include "ui.element.hpp"
#include "ui.label.hpp"
#include "ui.panel.hpp"
#include "ui.selection.hpp"
#include "ui.slider.hpp"

namespace experimental::ui_decl {

class BorderedStack final : public UiElement {
public:
    explicit BorderedStack(UiElementMetadata metadata);

    void SetGap(float gap);
    void SetPadding(UiThickness padding);

    template <typename T>
    T* AddItem(std::unique_ptr<T> child, float fixed_main_size = 0.0f)
    {
        return panel_->AddItem(std::move(child), fixed_main_size);
    }

    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const override;
    void Arrange(D2D1_RECT_F final_rect) override;
    void Render(const UiDrawContext& context, UiRootState state) const override;

private:
    StackPanel* panel_ = nullptr;
};

UiElementMetadata PaneMetadata(const wchar_t* automation_id = L"");
UiElementMetadata TextMetadata(const wchar_t* text, const wchar_t* automation_id = L"");

std::unique_ptr<Label> Title(const wchar_t* text, const wchar_t* automation_id = L"");
std::unique_ptr<Label> Body(const wchar_t* text, const wchar_t* automation_id = L"");
std::unique_ptr<Label> Muted(const wchar_t* text, const wchar_t* automation_id = L"");
std::unique_ptr<::Button> ActionButton(
    UiAction action,
    const wchar_t* name,
    const wchar_t* automation_id,
    const wchar_t* icon,
    const wchar_t* text);
std::unique_ptr<::Button> Button(
    const wchar_t* name,
    const wchar_t* automation_id,
    const wchar_t* icon,
    const wchar_t* text);
std::unique_ptr<::Checkbox> Toggle(
    const wchar_t* text,
    const wchar_t* automation_id,
    bool checked);
std::unique_ptr<::SliderRow> SliderField(
    const wchar_t* name,
    const wchar_t* automation_id,
    int minimum,
    int maximum,
    int value,
    int small_step,
    int large_step);

template <typename... Children>
std::unique_ptr<StackPanel> VStack(Children... children)
{
    auto panel = std::make_unique<StackPanel>(PaneMetadata());
    (panel->AddItem(std::move(children)), ...);
    return panel;
}

template <typename... Children>
std::unique_ptr<StackPanel> Group(Children... children)
{
    auto panel = VStack(std::move(children)...);
    panel->SetGap(0.0f);
    return panel;
}

template <typename... Children>
std::unique_ptr<BorderedStack> BorderBox(Children... children)
{
    auto panel = std::make_unique<BorderedStack>(PaneMetadata());
    (panel->AddItem(std::move(children)), ...);
    return panel;
}

std::unique_ptr<StackPanel> Section(
    const wchar_t* title,
    std::unique_ptr<UiElement> content,
    const wchar_t* title_automation_id = L"",
    const wchar_t* section_automation_id = L"");

} // namespace experimental::ui_decl
