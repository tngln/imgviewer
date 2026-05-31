#include "ui.text.hpp"

#include <string>

#include <wil/com.h>

namespace {

constexpr float kMeasureMaxWidth = 10000.0f;
constexpr float kMeasureMaxHeight = 1000.0f;
constexpr wchar_t kEllipsis[] = L"...";

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

std::wstring TruncateText(
    IDWriteFactory* factory,
    IDWriteTextFormat* text_format,
    const wchar_t* text,
    UINT32 text_length,
    float max_width)
{
    if (text == nullptr || text_length == 0 || max_width <= 0.0f) {
        return {};
    }

    const TextMetrics full_metrics = MeasureText(factory, text_format, text, text_length);
    if (full_metrics.width <= max_width) {
        return std::wstring(text, text + text_length);
    }

    constexpr UINT32 ellipsis_length = static_cast<UINT32>(sizeof(kEllipsis) / sizeof(kEllipsis[0]) - 1);
    const TextMetrics ellipsis_metrics = MeasureText(factory, text_format, kEllipsis, ellipsis_length);
    if (ellipsis_metrics.width > max_width) {
        return {};
    }

    size_t best = 0;
    size_t low = 0;
    size_t high = text_length;
    while (low <= high) {
        const size_t mid = low + (high - low) / 2;
        std::wstring candidate(text, text + mid);
        candidate += kEllipsis;

        const TextMetrics candidate_metrics =
            MeasureText(factory, text_format, candidate.c_str(), static_cast<UINT32>(candidate.size()));
        if (candidate_metrics.width <= max_width) {
            best = mid;
            low = mid + 1;
        } else {
            if (mid == 0) {
                break;
            }
            high = mid - 1;
        }
    }

    std::wstring truncated(text, text + best);
    truncated += kEllipsis;
    return truncated;
}

} // namespace ui_text
