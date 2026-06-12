#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace util {

template <typename T>
class Signal {
public:
    Signal() = delete;
};

template <>
class Signal<bool> {
public:
    using ValueType = bool;
    using Listener = std::function<void(bool)>;

    explicit Signal(bool initial_value = false);

    bool Get() const;
    bool Set(bool value);
    size_t Subscribe(Listener listener);
    void Unsubscribe(size_t subscription_id);

private:
    struct ListenerEntry final {
        size_t id = 0;
        Listener listener;
    };

    bool value_ = false;
    size_t next_subscription_id_ = 1;
    std::vector<ListenerEntry> listeners_;
};

template <>
class Signal<int> {
public:
    using ValueType = int;
    using Listener = std::function<void(int)>;

    explicit Signal(int initial_value = 0);

    int Get() const;
    bool Set(int value);
    size_t Subscribe(Listener listener);
    void Unsubscribe(size_t subscription_id);

private:
    struct ListenerEntry final {
        size_t id = 0;
        Listener listener;
    };

    int value_ = 0;
    size_t next_subscription_id_ = 1;
    std::vector<ListenerEntry> listeners_;
};

template <>
class Signal<std::wstring> {
public:
    using ValueType = std::wstring;
    using Listener = std::function<void(const std::wstring&)>;

    explicit Signal(std::wstring initial_value = {});

    const std::wstring& Get() const;
    bool Set(std::wstring value);
    size_t Subscribe(Listener listener);
    void Unsubscribe(size_t subscription_id);

private:
    struct ListenerEntry final {
        size_t id = 0;
        Listener listener;
    };

    std::wstring value_;
    size_t next_subscription_id_ = 1;
    std::vector<ListenerEntry> listeners_;
};

} // namespace util
