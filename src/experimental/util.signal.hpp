#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace util {

// A single observable value with change notifications.
//
// Design notes (see refactor.md S1/S2):
//   * One generic template for any equality-comparable, copyable T (bool, int,
//     enum, float, D2D1_COLOR_F, std::wstring, ...). No per-type specialization.
//   * Set() is a no-op (returns false, no notification) when the value is equal,
//     which also breaks two-way binding feedback loops.
//   * Notification is re-entrancy safe: subscribing or unsubscribing from inside
//     a listener does not corrupt the listener list. Listeners added during a
//     notification do not fire until the next notification; unsubscribed
//     listeners are tombstoned and physically removed once notification settles.
template <typename T>
class Signal {
public:
    using ValueType = T;
    using Listener = std::function<void(const T&)>;

    Signal() = default;
    explicit Signal(T initial_value) : value_(std::move(initial_value)) {}

    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;
    Signal(Signal&&) = default;
    Signal& operator=(Signal&&) = default;

    const T& Get() const { return value_; }

    bool Set(T value)
    {
        if (value_ == value) {
            return false;
        }
        value_ = std::move(value);
        Notify();
        return true;
    }

    size_t Subscribe(Listener listener)
    {
        const size_t subscription_id = next_subscription_id_++;
        listeners_.push_back(ListenerEntry{
            .id = subscription_id,
            .listener = std::move(listener),
        });
        return subscription_id;
    }

    void Unsubscribe(size_t subscription_id)
    {
        for (ListenerEntry& entry : listeners_) {
            if (entry.id == subscription_id) {
                entry.listener = nullptr; // tombstone; compacted after notification settles
            }
        }
        if (notifying_ == 0) {
            Compact();
        }
    }

private:
    struct ListenerEntry final {
        size_t id = 0;
        Listener listener;
    };

    void Notify()
    {
        ++notifying_;
        // Snapshot the count so listeners added during this notification do not
        // fire until the next one; index iteration tolerates the vector growing.
        const size_t count = listeners_.size();
        for (size_t index = 0; index < count; ++index) {
            const Listener& listener = listeners_[index].listener;
            if (listener) {
                listener(value_);
            }
        }
        --notifying_;
        if (notifying_ == 0) {
            Compact();
        }
    }

    void Compact()
    {
        listeners_.erase(
            std::remove_if(
                listeners_.begin(),
                listeners_.end(),
                [](const ListenerEntry& entry) { return !entry.listener; }),
            listeners_.end());
    }

    T value_{};
    size_t next_subscription_id_ = 1;
    int notifying_ = 0;
    std::vector<ListenerEntry> listeners_;
};

} // namespace util
