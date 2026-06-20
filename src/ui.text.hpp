#pragma once

#include <string>

#include <dwrite.h>

namespace ui_text {

struct TypeFace final {
    std::wstring family = L"Segoe UI";
    float size = 14.0f;
    DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_SEMI_BOLD;
    DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL;
    DWRITE_FONT_STRETCH stretch = DWRITE_FONT_STRETCH_NORMAL;
};

struct TextMetrics final {
    float width = 0.0f;
    float height = 0.0f;
};

HRESULT CreateTextFormat(
    IDWriteFactory* factory,
    const TypeFace& typeface,
    IDWriteTextFormat** format);
TextMetrics GetTextMetrics(
    IDWriteFactory* factory,
    IDWriteTextFormat* text_format,
    const wchar_t* text,
    UINT32 text_length);
std::wstring TruncateText(
    IDWriteFactory* factory,
    IDWriteTextFormat* text_format,
    const wchar_t* text,
    UINT32 text_length,
    float max_width);

} // namespace ui_text
