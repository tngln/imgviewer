#include "util.signal.hpp"

#include <algorithm>
#include <utility>

namespace util {

namespace {

template <typename ListenerList, typename Value>
void NotifyListeners(const ListenerList& listeners, Value&& value)
{
    for (const auto& entry : listeners) {
        if (entry.listener) {
            entry.listener(std::forward<Value>(value));
        }
    }
}

template <typename ListenerList>
void EraseSubscription(ListenerList* listeners, size_t subscription_id)
{
    if (listeners == nullptr) {
        return;
    }

    listeners->erase(std::remove_if(listeners->begin(), listeners->end(), [subscription_id](const auto& entry) {
        return entry.id == subscription_id;
    }), listeners->end());
}

} // namespace

Signal<bool>::Signal(bool initial_value) : value_(initial_value) {}

bool Signal<bool>::Get() const
{
    return value_;
}

bool Signal<bool>::Set(bool value)
{
    if (value_ == value) {
        return false;
    }

    value_ = value;
    NotifyListeners(listeners_, value_);
    return true;
}

size_t Signal<bool>::Subscribe(Listener listener)
{
    const size_t subscription_id = next_subscription_id_++;
    listeners_.push_back(ListenerEntry{
        .id = subscription_id,
        .listener = std::move(listener),
    });
    return subscription_id;
}

void Signal<bool>::Unsubscribe(size_t subscription_id)
{
    EraseSubscription(&listeners_, subscription_id);
}

Signal<int>::Signal(int initial_value) : value_(initial_value) {}

int Signal<int>::Get() const
{
    return value_;
}

bool Signal<int>::Set(int value)
{
    if (value_ == value) {
        return false;
    }

    value_ = value;
    NotifyListeners(listeners_, value_);
    return true;
}

size_t Signal<int>::Subscribe(Listener listener)
{
    const size_t subscription_id = next_subscription_id_++;
    listeners_.push_back(ListenerEntry{
        .id = subscription_id,
        .listener = std::move(listener),
    });
    return subscription_id;
}

void Signal<int>::Unsubscribe(size_t subscription_id)
{
    EraseSubscription(&listeners_, subscription_id);
}

Signal<std::wstring>::Signal(std::wstring initial_value) : value_(std::move(initial_value)) {}

const std::wstring& Signal<std::wstring>::Get() const
{
    return value_;
}

bool Signal<std::wstring>::Set(std::wstring value)
{
    if (value_ == value) {
        return false;
    }

    value_ = std::move(value);
    NotifyListeners(listeners_, value_);
    return true;
}

size_t Signal<std::wstring>::Subscribe(Listener listener)
{
    const size_t subscription_id = next_subscription_id_++;
    listeners_.push_back(ListenerEntry{
        .id = subscription_id,
        .listener = std::move(listener),
    });
    return subscription_id;
}

void Signal<std::wstring>::Unsubscribe(size_t subscription_id)
{
    EraseSubscription(&listeners_, subscription_id);
}

} // namespace util
