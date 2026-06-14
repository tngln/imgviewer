#pragma once

#include <optional>
#include <string_view>

#include <d2d1_1.h>

namespace script {

std::optional<D2D1_COLOR_F> ParseCanvasColor(std::string_view color);

} // namespace script
