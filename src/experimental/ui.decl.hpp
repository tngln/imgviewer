#pragma once

#include <memory>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

#include "experimental/ui.bind.hpp"
#include "ui.action.hpp"
#include "ui.button.hpp"
#include "ui.element.hpp"
#include "ui.label.hpp"
#include "ui.panel.hpp"
#include "ui.selection.hpp"
#include "ui.slider.hpp"

namespace experimental::ui_decl {

template <typename T>
class Owned final {
public:
    explicit Owned(std::unique_ptr<T> value) : value_(std::move(value)) {}

    Owned(Owned&&) noexcept = default;
    Owned& operator=(Owned&&) noexcept = default;
    Owned(const Owned&) = delete;
    Owned& operator=(const Owned&) = delete;

    T* Get() const { return value_.get(); }
    T* operator->() const { return value_.get(); }

    std::unique_ptr<T> Release() { return std::move(value_); }
    operator std::unique_ptr<T>() && { return std::move(value_); }
    template <typename U>
        requires(std::is_convertible_v<T*, U*> && !std::is_same_v<T, U>)
    operator std::unique_ptr<U>() &&
    {
        return std::unique_ptr<U>(std::move(value_));
    }

    Owned& Capture(T*& slot) &
    {
        slot = value_.get();
        return *this;
    }

    Owned&& Capture(T*& slot) &&
    {
        slot = value_.get();
        return std::move(*this);
    }

    template <typename F>
    Owned& Capture(F&& capture) &
        requires requires(F&& fn, T* value) { fn(value); }
    {
        capture(value_.get());
        return *this;
    }

    template <typename F>
    Owned&& Capture(F&& capture) &&
        requires requires(F&& fn, T* value) { fn(value); }
    {
        capture(value_.get());
        return std::move(*this);
    }

    Owned& Gap(float gap) &
        requires requires(T& value) { value.SetGap(gap); }
    {
        value_->SetGap(gap);
        return *this;
    }

    Owned&& Gap(float gap) &&
        requires requires(T& value) { value.SetGap(gap); }
    {
        value_->SetGap(gap);
        return std::move(*this);
    }

    Owned& Padding(UiThickness padding) &
        requires requires(T& value) { value.SetPadding(padding); }
    {
        value_->SetPadding(padding);
        return *this;
    }

    Owned&& Padding(UiThickness padding) &&
        requires requires(T& value) { value.SetPadding(padding); }
    {
        value_->SetPadding(padding);
        return std::move(*this);
    }

    Owned& CrossAxis(ui_layout::StackCrossAxis cross_axis) &
        requires requires(T& value) { value.SetCrossAxis(cross_axis); }
    {
        value_->SetCrossAxis(cross_axis);
        return *this;
    }

    Owned&& CrossAxis(ui_layout::StackCrossAxis cross_axis) &&
        requires requires(T& value) { value.SetCrossAxis(cross_axis); }
    {
        value_->SetCrossAxis(cross_axis);
        return std::move(*this);
    }

private:
    std::unique_ptr<T> value_;
};

class BoundBuilder final {
public:
    explicit BoundBuilder(ui_bind::SubscriptionBag& subscriptions);

    Owned<::Checkbox> Toggle(
        const wchar_t* text,
        util::Signal<bool>& signal) const;
    Owned<::RadioButton> Radio(
        const wchar_t* text,
        util::Signal<bool>& signal,
        bool selected_value) const;
    Owned<::RadioButton> Radio(
        const wchar_t* text,
        util::Signal<int>& signal,
        int selected_value) const;
    Owned<::SliderRow> SliderField(
        const wchar_t* name,
        int minimum,
        int maximum,
        util::Signal<int>& signal,
        int small_step,
        int large_step,
        std::function<int(int)> normalize,
        std::function<std::wstring(int)> format_value) const;
    Owned<::Dropdown> DropdownField(
        const wchar_t* name,
        std::vector<DropdownOption> options,
        util::Signal<int>& signal) const;

private:
    ui_bind::SubscriptionBag* subscriptions_ = nullptr;
};

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

    template <typename TOwned>
    auto AddItem(TOwned child, float fixed_main_size = 0.0f)
        -> decltype(this->AddItem(child.Release(), fixed_main_size))
    {
        return AddItem(child.Release(), fixed_main_size);
    }

    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const override;
    void Arrange(D2D1_RECT_F final_rect) override;
    void Render(const UiDrawContext& context, UiRootState state) const override;

private:
    StackPanel* panel_ = nullptr;
};

UiElementMetadata PaneMetadata();
UiElementMetadata TextMetadata(const wchar_t* text);
BoundBuilder Bind(ui_bind::SubscriptionBag& subscriptions);

std::unique_ptr<Label> Title(const wchar_t* text);
std::unique_ptr<Label> Body(const wchar_t* text);
std::unique_ptr<Label> Muted(const wchar_t* text);
std::unique_ptr<::Button> ActionButton(
    UiAction action,
    const wchar_t* name,
    const wchar_t* icon,
    const wchar_t* text);
std::unique_ptr<::Button> Button(
    const wchar_t* name,
    const wchar_t* icon,
    const wchar_t* text);
std::unique_ptr<::Checkbox> Toggle(
    const wchar_t* text,
    bool checked);
std::unique_ptr<::Checkbox> Toggle(
    const wchar_t* text,
    util::Signal<bool>& signal,
    ui_bind::SubscriptionBag& subscriptions);
std::unique_ptr<::RadioButton> Radio(
    const wchar_t* text,
    bool checked);
std::unique_ptr<::RadioButton> Radio(
    const wchar_t* text,
    util::Signal<bool>& signal,
    bool selected_value,
    ui_bind::SubscriptionBag& subscriptions);
std::unique_ptr<::RadioButton> Radio(
    const wchar_t* text,
    util::Signal<int>& signal,
    int selected_value,
    ui_bind::SubscriptionBag& subscriptions);
std::unique_ptr<::SliderRow> SliderField(
    const wchar_t* name,
    int minimum,
    int maximum,
    int value,
    int small_step,
    int large_step);
std::unique_ptr<::SliderRow> SliderField(
    const wchar_t* name,
    int minimum,
    int maximum,
    util::Signal<int>& signal,
    int small_step,
    int large_step,
    std::function<int(int)> normalize,
    std::function<std::wstring(int)> format_value,
    ui_bind::SubscriptionBag& subscriptions);
std::unique_ptr<::Dropdown> DropdownField(
    const wchar_t* name,
    std::vector<DropdownOption> options,
    util::Signal<int>& signal,
    ui_bind::SubscriptionBag& subscriptions);

template <typename... Children>
Owned<StackPanel> VStack(Children... children)
{
    auto panel = std::make_unique<StackPanel>(PaneMetadata());
    (panel->AddItem(std::move(children)), ...);
    return Owned<StackPanel>(std::move(panel));
}

template <typename... Children>
Owned<StackPanel> VStack(UiElementMetadata metadata, Children... children)
{
    auto panel = std::make_unique<StackPanel>(metadata);
    (panel->AddItem(std::move(children)), ...);
    return Owned<StackPanel>(std::move(panel));
}

template <typename... Children>
Owned<StackPanel> Group(Children... children)
{
    auto panel = VStack(std::move(children)...);
    panel->SetGap(0.0f);
    return panel;
}

template <typename... Children>
Owned<StackPanel> HStack(Children... children)
{
    auto panel = std::make_unique<StackPanel>(PaneMetadata(), ui_layout::StackDirection::Horizontal);
    (panel->AddItem(std::move(children)), ...);
    return Owned<StackPanel>(std::move(panel));
}

template <typename... Children>
Owned<StackPanel> HStack(UiElementMetadata metadata, Children... children)
{
    auto panel = std::make_unique<StackPanel>(metadata, ui_layout::StackDirection::Horizontal);
    (panel->AddItem(std::move(children)), ...);
    return Owned<StackPanel>(std::move(panel));
}

template <typename... Children>
Owned<BorderedStack> BorderBox(Children... children)
{
    auto panel = std::make_unique<BorderedStack>(PaneMetadata());
    (panel->AddItem(std::move(children)), ...);
    return Owned<BorderedStack>(std::move(panel));
}

Owned<StackPanel> Section(
    const wchar_t* title,
    std::unique_ptr<UiElement> content);

template <typename TContent>
Owned<StackPanel> Section(
    const wchar_t* title,
    TContent content)
    requires requires(TContent&& value) {
        static_cast<std::unique_ptr<UiElement>>(std::move(value));
    }
{
    return Section(title, static_cast<std::unique_ptr<UiElement>>(std::move(content)));
}

} // namespace experimental::ui_decl
