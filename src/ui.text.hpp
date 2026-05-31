#pragma once

#include <dwrite.h>

namespace ui_text {

struct TextMetrics final {
    float width = 0.0f;
    float height = 0.0f;
};

TextMetrics MeasureText(
    IDWriteFactory* factory,
    IDWriteTextFormat* text_format,
    const wchar_t* text,
    UINT32 text_length);

} // namespace ui_text
