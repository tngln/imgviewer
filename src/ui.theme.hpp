#pragma once

#include <d2d1helper.h>

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
constexpr auto kTitleBarBackground = COLOR(0xffffff);
constexpr float kTitleBarBackgroundOpacity = 0.86f;
constexpr auto kBorder = COLOR(0xb8c7dc);
constexpr auto kBodyText = COLOR(0x172033);
constexpr auto kMutedText = COLOR(0x697386);
constexpr auto kAccent = COLOR(0x2f6fed);
constexpr auto kButtonDefault = COLOR(0xffffff);
constexpr auto kButtonHovered = COLOR(0xebf2ff);
constexpr auto kButtonPressed = COLOR(0xdbe7ff);
constexpr auto kDangerHovered = COLOR(0xffdad6);
constexpr auto kDangerPressed = COLOR(0xf2b8b5);
constexpr auto kToolbarBackground = COLOR(0xffffff);
constexpr float kToolbarBackgroundOpacity = 0.88f;

} // namespace color

namespace metrics {

constexpr float kTitleBarHeight = 48.0f;
constexpr float kCaptionButtonWidth = 48.0f;
constexpr float kCaptionButtonEdgePadding = 1.0f;
constexpr float kTitleTextLeft = 16.0f;
constexpr float kTitleTextRightPadding = 12.0f;
constexpr float kWindowBorderInset = 0.5f;
constexpr float kWindowBorderMinimum = 0.5f;
constexpr float kButtonCornerRadius = 6.0f;
constexpr float kPanelPadding = 32.0f;
constexpr float kPrimaryButtonTop = 128.0f;
constexpr float kPrimaryButtonHeight = 44.0f;
constexpr float kOpenButtonWidth = 200.0f;
constexpr float kToolbarButtonSize = 44.0f;
constexpr float kToolbarButtonGap = 2.0f;
constexpr float kToolbarPadding = 8.0f;
constexpr float kToolbarBottomMargin = 28.0f;
constexpr float kToolbarCornerRadius = 8.0f;
constexpr float kBodyTextTop = 58.0f;
constexpr float kBodyTextBottom = 98.0f;
constexpr float kIconTextTop = 96.0f;
constexpr float kIconTextBottom = 136.0f;
constexpr float kIconPlaceholderSize = 96.0f;
constexpr float kIconPlaceholderPadding = 24.0f;
constexpr float kIconPlaceholderMinimumSize = 16.0f;
constexpr float kPathIconStrokeWidth = 1.75f;

} // namespace metrics

namespace offset {

constexpr float kButtonIconLeft = 14.0f;
constexpr float kButtonIconTop = 10.0f;
constexpr float kButtonIconRight = 38.0f;
constexpr float kButtonTextLeft = 44.0f;
constexpr float kButtonTextTop = 5.0f;
constexpr float kButtonTextRight = 12.0f;
constexpr float kCaptionIconLeft = 12.0f;
constexpr float kCaptionIconTop = 10.0f;

} // namespace offset

} // namespace ui_theme
