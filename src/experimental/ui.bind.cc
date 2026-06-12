#include "experimental/ui.bind.hpp"

#include <algorithm>
#include <utility>

namespace experimental::ui_bind {

SubscriptionBag::~SubscriptionBag()
{
    for (auto it = unsubscribers_.rbegin(); it != unsubscribers_.rend(); ++it) {
        (*it)();
    }
}

void SubscriptionBag::Add(util::Signal<bool>& signal, size_t subscription_id)
{
    unsubscribers_.push_back([&signal, subscription_id]() {
        signal.Unsubscribe(subscription_id);
    });
}

void SubscriptionBag::Add(util::Signal<int>& signal, size_t subscription_id)
{
    unsubscribers_.push_back([&signal, subscription_id]() {
        signal.Unsubscribe(subscription_id);
    });
}

void SubscriptionBag::Add(util::Signal<std::wstring>& signal, size_t subscription_id)
{
    unsubscribers_.push_back([&signal, subscription_id]() {
        signal.Unsubscribe(subscription_id);
    });
}

void BindCheckbox(Checkbox& checkbox, util::Signal<bool>& signal, SubscriptionBag& subscriptions)
{
    checkbox.SetChecked(signal.Get());
    checkbox.SetOnToggled([&signal](bool checked) {
        signal.Set(checked);
    });
    subscriptions.Add(signal, signal.Subscribe([&checkbox](bool checked) {
        checkbox.SetChecked(checked);
    }));
}

void BindRadioBool(RadioButton& radio, util::Signal<bool>& signal, bool selected_value, SubscriptionBag& subscriptions)
{
    radio.SetSelected(signal.Get() == selected_value);
    radio.SetOnSelected([&signal, selected_value]() {
        signal.Set(selected_value);
    });
    subscriptions.Add(signal, signal.Subscribe([&radio, selected_value](bool value) {
        radio.SetSelected(value == selected_value);
    }));
}

void BindRadioInt(RadioButton& radio, util::Signal<int>& signal, int selected_value, SubscriptionBag& subscriptions)
{
    radio.SetSelected(signal.Get() == selected_value);
    radio.SetOnSelected([&signal, selected_value]() {
        signal.Set(selected_value);
    });
    subscriptions.Add(signal, signal.Subscribe([&radio, selected_value](int value) {
        radio.SetSelected(value == selected_value);
    }));
}

void BindDropdownIndex(Dropdown& dropdown, util::Signal<int>& signal, SubscriptionBag& subscriptions)
{
    dropdown.SetSelectedIndex(static_cast<size_t>((std::max)(0, signal.Get())));
    dropdown.SetOnSelectionChanged([&signal](size_t index) {
        signal.Set(static_cast<int>(index));
    });
    subscriptions.Add(signal, signal.Subscribe([&dropdown](int value) {
        dropdown.SetSelectedIndex(static_cast<size_t>((std::max)(0, value)));
    }));
}

void BindSliderRow(
    SliderRow& row,
    util::Signal<int>& signal,
    std::function<int(int)> normalize,
    std::function<std::wstring(int)> format_value,
    SubscriptionBag& subscriptions)
{
    const auto apply_value = [&row, &format_value](int value) {
        row.SetValue(value);
        row.SetValueText(format_value(value).c_str());
    };
    const auto update_signal = [&signal, normalize](int value) {
        signal.Set(normalize(value));
    };

    apply_value(normalize(signal.Get()));
    row.GetSlider()->SetOnValueChanged(update_signal);
    row.GetSlider()->SetAccessibilityValueChangedHandler(update_signal);
    subscriptions.Add(signal, signal.Subscribe([apply_value, normalize](int value) {
        apply_value(normalize(value));
    }));
}

} // namespace experimental::ui_bind
