#include "ui.text.hpp"

#include <wil/com.h>

namespace {

constexpr float kMeasureMaxWidth = 10000.0f;
constexpr float kMeasureMaxHeight = 1000.0f;

} // namespace

namespace ui_text {

TextMetrics MeasureText(
    IDWriteFactory* factory,
    IDWriteTextFormat* text_format,
    const wchar_t* text,
    UINT32 text_length)
{
    if (factory == nullptr || text_format == nullptr || text == nullptr || text_length == 0) {
        return {};
    }

    wil::com_ptr<IDWriteTextLayout> layout;
    if (FAILED(factory->CreateTextLayout(
            text,
            text_length,
            text_format,
            kMeasureMaxWidth,
            kMeasureMaxHeight,
            layout.put()))) {
        return {};
    }

    DWRITE_TEXT_METRICS metrics = {};
    if (FAILED(layout->GetMetrics(&metrics))) {
        return {};
    }

    return TextMetrics{
        .width = metrics.widthIncludingTrailingWhitespace,
        .height = metrics.height,
    };
}

} // namespace ui_text
