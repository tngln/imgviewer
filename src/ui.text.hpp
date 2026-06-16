#pragma once

#include <string>

#include <dwrite.h>

namespace ui_text {

struct TextMetrics final {
    float width = 0.0f;
    float height = 0.0f;
};

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
