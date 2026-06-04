#pragma once

#include <type_traits>

struct UiAction final {
    int value = 0;

    constexpr UiAction() = default;
    constexpr UiAction(int action_value) : value(action_value) {}

    template <typename ActionEnum, typename = std::enable_if_t<std::is_enum_v<ActionEnum>>>
    constexpr UiAction(ActionEnum action) : value(static_cast<int>(action)) {}
};

constexpr bool operator==(UiAction left, UiAction right)
{
    return left.value == right.value;
}

constexpr bool operator!=(UiAction left, UiAction right)
{
    return !(left == right);
}

constexpr int UiActionValue(UiAction action)
{
    return action.value;
}

inline constexpr UiAction kUiActionNone{0};
inline constexpr UiAction kUiActionTextCopy{-1};
inline constexpr UiAction kUiActionTextCut{-2};
inline constexpr UiAction kUiActionTextPaste{-3};
inline constexpr UiAction kUiActionTextSelectAll{-4};
