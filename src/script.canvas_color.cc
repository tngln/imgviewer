#include "script.canvas_color.hpp"

#include <d2d1helper.h>

namespace script {
namespace {

int HexDigit(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

} // namespace

std::optional<D2D1_COLOR_F> ParseCanvasColor(std::string_view color)
{
    if (color.size() != 7 && color.size() != 9) {
        return std::nullopt;
    }
    if (color[0] != '#') {
        return std::nullopt;
    }

    auto byte_at = [&](size_t offset) -> std::optional<int> {
        const int hi = HexDigit(color[offset]);
        const int lo = HexDigit(color[offset + 1]);
        if (hi < 0 || lo < 0) {
            return std::nullopt;
        }
        return hi * 16 + lo;
    };

    int alpha = 255;
    size_t offset = 1;
    if (color.size() == 9) {
        std::optional<int> parsed = byte_at(offset);
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        alpha = *parsed;
        offset += 2;
    }

    const std::optional<int> red = byte_at(offset);
    const std::optional<int> green = byte_at(offset + 2);
    const std::optional<int> blue = byte_at(offset + 4);
    if (!red.has_value() || !green.has_value() || !blue.has_value()) {
        return std::nullopt;
    }

    return D2D1::ColorF(
        static_cast<float>(*red) / 255.0f,
        static_cast<float>(*green) / 255.0f,
        static_cast<float>(*blue) / 255.0f,
        static_cast<float>(alpha) / 255.0f);
}

} // namespace script
