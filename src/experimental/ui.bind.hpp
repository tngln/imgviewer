#pragma once

#include <functional>
#include <string>
#include <vector>

#include "experimental/util.signal.hpp"
#include "ui.selection.hpp"
#include "ui.slider.hpp"

namespace experimental::ui_bind {

class SubscriptionBag final {
public:
    SubscriptionBag() = default;
    SubscriptionBag(const SubscriptionBag&) = delete;
    SubscriptionBag& operator=(const SubscriptionBag&) = delete;
    SubscriptionBag(SubscriptionBag&&) = delete;
    SubscriptionBag& operator=(SubscriptionBag&&) = delete;
    ~SubscriptionBag();

    void Add(util::Signal<bool>& signal, size_t subscription_id);
    void Add(util::Signal<int>& signal, size_t subscription_id);
    void Add(util::Signal<std::wstring>& signal, size_t subscription_id);

private:
    std::vector<std::function<void()>> unsubscribers_;
};

void BindCheckbox(Checkbox& checkbox, util::Signal<bool>& signal, SubscriptionBag& subscriptions);
void BindRadioBool(RadioButton& radio, util::Signal<bool>& signal, bool selected_value, SubscriptionBag& subscriptions);
void BindRadioInt(RadioButton& radio, util::Signal<int>& signal, int selected_value, SubscriptionBag& subscriptions);
void BindDropdownIndex(Dropdown& dropdown, util::Signal<int>& signal, SubscriptionBag& subscriptions);
void BindSliderRow(
    SliderRow& row,
    util::Signal<int>& signal,
    std::function<int(int)> normalize,
    std::function<std::wstring(int)> format_value,
    SubscriptionBag& subscriptions);

} // namespace experimental::ui_bind
