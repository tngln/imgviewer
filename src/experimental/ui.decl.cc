#include "experimental/ui.decl.hpp"

#include <d2d1helper.h>

#include "ui.draw.hpp"
#include "ui.theme.hpp"

namespace experimental::ui_decl {
namespace {

constexpr float kSectionGap = 10.0f;
constexpr float kSectionTopPadding = 10.0f;

} // namespace

BoundBuilder::BoundBuilder(ui_bind::SubscriptionBag& subscriptions) : subscriptions_(&subscriptions) {}

Owned<::Checkbox> BoundBuilder::Toggle(
    const wchar_t* text,
    util::Signal<bool>& signal) const
{
    return Owned<::Checkbox>(ui_decl::Toggle(text, signal, *subscriptions_));
}

Owned<::RadioButton> BoundBuilder::Radio(
    const wchar_t* text,
    util::Signal<bool>& signal,
    bool selected_value) const
{
    return Owned<::RadioButton>(ui_decl::Radio(text, signal, selected_value, *subscriptions_));
}

Owned<::RadioButton> BoundBuilder::Radio(
    const wchar_t* text,
    util::Signal<int>& signal,
    int selected_value) const
{
    return Owned<::RadioButton>(ui_decl::Radio(text, signal, selected_value, *subscriptions_));
}

Owned<::SliderRow> BoundBuilder::SliderField(
    const wchar_t* name,
    int minimum,
    int maximum,
    util::Signal<int>& signal,
    int small_step,
    int large_step,
    std::function<int(int)> normalize,
    std::function<std::wstring(int)> format_value) const
{
    return Owned<::SliderRow>(ui_decl::SliderField(
        name,
        minimum,
        maximum,
        signal,
        small_step,
        large_step,
        std::move(normalize),
        std::move(format_value),
        *subscriptions_));
}

Owned<::Dropdown> BoundBuilder::DropdownField(
    const wchar_t* name,
    std::vector<DropdownOption> options,
    util::Signal<int>& signal) const
{
    return Owned<::Dropdown>(ui_decl::DropdownField(name, std::move(options), signal, *subscriptions_));
}

BorderedStack::BorderedStack(UiElementMetadata metadata) : UiElement(metadata)
{
    panel_ = static_cast<StackPanel*>(AddChild(std::make_unique<StackPanel>(PaneMetadata())));
}

void BorderedStack::SetGap(float gap)
{
    panel_->SetGap(gap);
}

void BorderedStack::SetPadding(UiThickness padding)
{
    panel_->SetPadding(padding);
}

D2D1_SIZE_F BorderedStack::Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const
{
    return panel_->Measure(context, available_size);
}

void BorderedStack::Arrange(D2D1_RECT_F final_rect)
{
    UiElement::Arrange(final_rect);
    panel_->Arrange(final_rect);
}

void BorderedStack::Render(const UiDrawContext& context, UiRootState state) const
{
    const UiDraw draw(context);
    draw.DrawRect(Rect(), ui_theme::color::kBorder);
    panel_->Render(context, state);
}

UiElementMetadata PaneMetadata()
{
    return UiMetadata(UiElementRole::Pane, L"", false, false);
}

UiElementMetadata TextMetadata(const wchar_t* text)
{
    return UiMetadata(UiElementRole::Text, text, false, false);
}

BoundBuilder Bind(ui_bind::SubscriptionBag& subscriptions)
{
    return BoundBuilder(subscriptions);
}

std::unique_ptr<Label> Title(const wchar_t* text)
{
    return std::make_unique<Label>(TextMetadata(text), text, LabelStyle::Title);
}

std::unique_ptr<Label> Body(const wchar_t* text)
{
    return std::make_unique<Label>(TextMetadata(text), text, LabelStyle::Body);
}

std::unique_ptr<Label> Muted(const wchar_t* text)
{
    return std::make_unique<Label>(TextMetadata(text), text, LabelStyle::Muted);
}

std::unique_ptr<::Button> ActionButton(
    UiAction action,
    const wchar_t* name,
    const wchar_t* icon,
    const wchar_t* text)
{
    return std::make_unique<::Button>(
        UiMetadata(UiElementRole::Button, action, name, kUiTooltipFromName),
        icon,
        text);
}

std::unique_ptr<::Button> Button(
    const wchar_t* name,
    const wchar_t* icon,
    const wchar_t* text)
{
    return std::make_unique<::Button>(
        UiMetadata(UiElementRole::Button, name, kUiTooltipFromName),
        icon,
        text);
}

std::unique_ptr<::Checkbox> Toggle(
    const wchar_t* text,
    bool checked)
{
    return std::make_unique<::Checkbox>(
        UiMetadata(UiElementRole::CheckBox, text, kUiTooltipFromName),
        text,
        checked);
}

std::unique_ptr<::Checkbox> Toggle(
    const wchar_t* text,
    util::Signal<bool>& signal,
    ui_bind::SubscriptionBag& subscriptions)
{
    auto checkbox = Toggle(text, signal.Get());
    ui_bind::BindCheckbox(*checkbox, signal, subscriptions);
    return checkbox;
}

std::unique_ptr<::RadioButton> Radio(
    const wchar_t* text,
    bool checked)
{
    return std::make_unique<::RadioButton>(
        UiMetadata(UiElementRole::RadioButton, text, kUiTooltipFromName),
        text,
        checked);
}

std::unique_ptr<::RadioButton> Radio(
    const wchar_t* text,
    util::Signal<bool>& signal,
    bool selected_value,
    ui_bind::SubscriptionBag& subscriptions)
{
    auto radio = Radio(text, signal.Get() == selected_value);
    ui_bind::BindRadioBool(*radio, signal, selected_value, subscriptions);
    return radio;
}

std::unique_ptr<::RadioButton> Radio(
    const wchar_t* text,
    util::Signal<int>& signal,
    int selected_value,
    ui_bind::SubscriptionBag& subscriptions)
{
    auto radio = Radio(text, signal.Get() == selected_value);
    ui_bind::BindRadioInt(*radio, signal, selected_value, subscriptions);
    return radio;
}

std::unique_ptr<::SliderRow> SliderField(
    const wchar_t* name,
    int minimum,
    int maximum,
    int value,
    int small_step,
    int large_step)
{
    return std::make_unique<::SliderRow>(
        UiMetadata(UiElementRole::Slider, name, kUiTooltipFromName),
        minimum,
        maximum,
        value,
        small_step,
        large_step);
}

std::unique_ptr<::SliderRow> SliderField(
    const wchar_t* name,
    int minimum,
    int maximum,
    util::Signal<int>& signal,
    int small_step,
    int large_step,
    std::function<int(int)> normalize,
    std::function<std::wstring(int)> format_value,
    ui_bind::SubscriptionBag& subscriptions)
{
    auto slider = SliderField(
        name,
        minimum,
        maximum,
        normalize(signal.Get()),
        small_step,
        large_step);
    ui_bind::BindSliderRow(*slider, signal, std::move(normalize), std::move(format_value), subscriptions);
    return slider;
}

std::unique_ptr<::Dropdown> DropdownField(
    const wchar_t* name,
    std::vector<DropdownOption> options,
    util::Signal<int>& signal,
    ui_bind::SubscriptionBag& subscriptions)
{
    auto dropdown = std::make_unique<::Dropdown>(
        UiMetadata(UiElementRole::ComboBox, name, kUiTooltipFromName),
        std::move(options));
    ui_bind::BindDropdownIndex(*dropdown, signal, subscriptions);
    return dropdown;
}

Owned<StackPanel> Section(
    const wchar_t* title,
    std::unique_ptr<UiElement> content)
{
    auto section = std::make_unique<StackPanel>(PaneMetadata());
    section->SetPadding(UiThickness{0.0f, kSectionTopPadding, 0.0f, 0.0f});
    section->SetGap(kSectionGap);
    section->AddItem(Body(title));
    section->AddItem(std::move(content));
    return Owned<StackPanel>(std::move(section));
}

} // namespace experimental::ui_decl
