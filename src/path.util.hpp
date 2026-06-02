#pragma once

#include <string>

namespace util {

std::wstring FileNameFromPath(const wchar_t* path, const wchar_t* fallback);

} // namespace util
