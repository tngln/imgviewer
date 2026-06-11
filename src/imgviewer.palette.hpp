#pragma once

#include <array>
#include <cstddef>

#include <d2d1_1.h>

#include "imgviewer.strings.hpp"

struct PaletteColor final {
    D2D1_COLOR_F color;
    ImgViewerStringId name;
};

struct PaletteEntry final {
    ImgViewerAction action;
    ImgViewerStringId name;
    ImgViewerStringId tooltip;
};

inline const std::array kPenColors = {
    PaletteColor{D2D1::ColorF(D2D1::ColorF::Red), ImgViewerStringId::Red},
    PaletteColor{D2D1::ColorF(D2D1::ColorF::Yellow), ImgViewerStringId::Yellow},
    PaletteColor{D2D1::ColorF(D2D1::ColorF::Lime), ImgViewerStringId::Green},
    PaletteColor{D2D1::ColorF(D2D1::ColorF::Cyan), ImgViewerStringId::Cyan},
    PaletteColor{D2D1::ColorF(D2D1::ColorF::DodgerBlue), ImgViewerStringId::Blue},
    PaletteColor{D2D1::ColorF(D2D1::ColorF::Magenta), ImgViewerStringId::Magenta},
    PaletteColor{D2D1::ColorF(D2D1::ColorF::White), ImgViewerStringId::White},
    PaletteColor{D2D1::ColorF(D2D1::ColorF::Black), ImgViewerStringId::Black},
};

constexpr std::array kPenWidths = {2.0f, 4.0f, 8.0f, 12.0f};

constexpr std::array kTextSizes = {12.0f, 16.0f, 20.0f, 28.0f, 36.0f};

inline const std::array kTextBackgrounds = {
    PaletteColor{D2D1::ColorF(D2D1::ColorF::Yellow, 0.82f), ImgViewerStringId::Yellow},
    PaletteColor{D2D1::ColorF(D2D1::ColorF::White, 0.82f), ImgViewerStringId::White},
    PaletteColor{D2D1::ColorF(D2D1::ColorF::Black, 0.82f), ImgViewerStringId::Black},
    PaletteColor{D2D1::ColorF(D2D1::ColorF::Red, 0.82f), ImgViewerStringId::Red},
    PaletteColor{D2D1::ColorF(D2D1::ColorF::DodgerBlue, 0.82f), ImgViewerStringId::Blue},
};

constexpr size_t kPenColorCount = kPenColors.size();
constexpr size_t kPenWidthCount = kPenWidths.size();
constexpr size_t kTextSizeCount = kTextSizes.size();
constexpr size_t kTextBackgroundCount = kTextBackgrounds.size();

inline int32_t PackColor(D2D1_COLOR_F c)
{
    return (static_cast<int32_t>(c.r * 255.0f + 0.5f) << 24) |
        (static_cast<int32_t>(c.g * 255.0f + 0.5f) << 16) |
        (static_cast<int32_t>(c.b * 255.0f + 0.5f) << 8) |
        static_cast<int32_t>(c.a * 255.0f + 0.5f);
}

inline D2D1_COLOR_F UnpackColor(int32_t arg)
{
    return D2D1::ColorF(
        static_cast<float>((arg >> 24) & 0xFF) / 255.0f,
        static_cast<float>((arg >> 16) & 0xFF) / 255.0f,
        static_cast<float>((arg >> 8) & 0xFF) / 255.0f,
        static_cast<float>(arg & 0xFF) / 255.0f);
}

inline constexpr int32_t PackFloat(float value)
{
    return static_cast<int32_t>(value * 100.0f + 0.5f);
}

inline constexpr float UnpackFloat(int32_t arg)
{
    return static_cast<float>(arg) / 100.0f;
}
