#pragma once

#include <d2d1helper.h>

#include "ui.element.hpp"

namespace ui_theme::detail {

constexpr float ColorChannel(unsigned int value, unsigned int shift)
{
    return static_cast<float>((value >> shift) & 0xffU) / 255.0f;
}

} // namespace ui_theme::detail

#define COLOR(value) \
    D2D1_COLOR_F { \
        ui_theme::detail::ColorChannel((value), 16U), ui_theme::detail::ColorChannel((value), 8U), \
            ui_theme::detail::ColorChannel((value), 0U), 1.0f \
    }

namespace ui_theme {

namespace color {

constexpr auto kWindowBackground = COLOR(0xf7f9fc);
constexpr auto kCheckerboardLight = COLOR(0xf1f4f8);
constexpr auto kCheckerboardDark = COLOR(0xcfd8e6);
constexpr auto kTitleBarBackground = COLOR(0xffffff);
constexpr float kTitleBarBackgroundOpacity = 0.86f;
constexpr auto kBorder = COLOR(0xb8c7dc);
constexpr auto kEditModeBorder = COLOR(0x8b3ff2);
constexpr auto kBodyText = COLOR(0x172033);
constexpr auto kMutedText = COLOR(0x697386);
constexpr auto kAccent = COLOR(0x2f6fed);
constexpr auto kButtonDefault = COLOR(0xffffff);
constexpr auto kButtonHovered = COLOR(0xebf2ff);
constexpr auto kButtonPressed = COLOR(0xdbe7ff);
constexpr auto kButtonDisabled = COLOR(0xf3f6fa);
constexpr auto kButtonDisabledContent = COLOR(0x9aa8bd);
constexpr auto kDangerHovered = COLOR(0xffdad6);
constexpr auto kDangerPressed = COLOR(0xf2b8b5);
constexpr auto kToolbarBackground = COLOR(0xffffff);
constexpr float kToolbarBackgroundOpacity = 0.88f;

} // namespace color

namespace metrics {

constexpr float kHalfPixel = 0.5f;
constexpr float kSmallGap = 4.0f;
constexpr float kStandardGap = 6.0f;
constexpr float kLargeGap = 8.0f;
constexpr float kSectionPadding = 12.0f;
constexpr float kTextTopOffset = 2.5f;

constexpr float kTitleBarHeight = 24.0f;
constexpr float kCaptionButtonWidth = 24.0f;
constexpr float kTitleTextLeft = 8.0f;
constexpr float kTitleTextRightPadding = 6.0f;
constexpr float kWindowBorderInset = 0.25f;
constexpr float kWindowBorderMinimum = 0.25f;
constexpr float kButtonCornerRadius = 3.0f;
constexpr float kPanelPadding = 16.0f;
constexpr float kPrimaryButtonTop = 64.0f;
constexpr float kPrimaryButtonHeight = 22.0f;
constexpr float kOpenButtonWidth = 100.0f;
constexpr float kToolbarButtonSize = 22.0f;
constexpr float kToolbarButtonGap = 1.0f;
constexpr float kToolbarPadding = 4.0f;
constexpr float kToolbarDragHandleWidth = 9.0f;
constexpr float kToolbarBottomMargin = 14.0f;
constexpr float kToolbarCornerRadius = 4.0f;
constexpr float kBodyTextTop = 29.0f;
constexpr float kBodyTextBottom = 49.0f;
constexpr float kIconTextTop = 48.0f;
constexpr float kIconTextBottom = 68.0f;
constexpr float kIconPlaceholderSize = 48.0f;
constexpr float kIconPlaceholderPadding = 12.0f;
constexpr float kIconPlaceholderMinimumSize = 8.0f;
constexpr float kPathIconStrokeWidth = 0.875f;
constexpr float kStrokeWidth = 1.0f;
constexpr float kActiveStrokeWidth = 1.5f;
constexpr float kWidgetRowHeight = 18.0f;
constexpr float kInputHeight = 21.0f;
constexpr float kTextRowTopOffset = 3.0f;
constexpr float kPanelCornerRadius = 4.0f;

} // namespace metrics

namespace offset {

constexpr float kButtonIconLeft = 7.0f;
constexpr float kButtonIconTop = 5.0f;
constexpr float kButtonIconRight = 19.0f;
constexpr float kButtonTextLeft = 22.0f;
constexpr float kButtonTextRight = 6.0f;
constexpr float kCaptionIconLeft = 6.0f;
constexpr float kCaptionIconTop = 5.0f;

} // namespace offset

inline D2D1_COLOR_F WidgetFillColor(UiElementState state)
{
    if (!state.enabled) {
        return ui_theme::color::kButtonDisabled;
    }
    if (state.danger && (state.hovered || state.pressed)) {
        return state.pressed ? ui_theme::color::kDangerPressed : ui_theme::color::kDangerHovered;
    }
    if (state.pressed) {
        return ui_theme::color::kButtonPressed;
    }
    if (state.hovered || state.active || state.expanded) {
        return ui_theme::color::kButtonHovered;
    }
    return ui_theme::color::kButtonDefault;
}

} // namespace ui_theme
