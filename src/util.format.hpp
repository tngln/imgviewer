#pragma once

#include <windows.h>

#include <d2d1_1.h>

#include <string>

namespace util {

std::wstring FormatFileSize(ULONGLONG byte_count);
std::wstring FormatFileTime(FILETIME file_time);
std::wstring FormatImageDimensions(D2D1_SIZE_U size);
std::wstring FormatImageType(const std::wstring& path, bool clipboard);

} // namespace util
