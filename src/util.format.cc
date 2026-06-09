#include "util.format.hpp"

#include <algorithm>
#include <cwctype>
#include <filesystem>

#include "imgviewer.strings.hpp"

namespace util {

std::wstring FormatFileSize(ULONGLONG byte_count)
{
    constexpr ULONGLONG kKiB = 1024;
    constexpr ULONGLONG kMiB = kKiB * 1024;
    constexpr ULONGLONG kGiB = kMiB * 1024;

    wchar_t text[64] = {};
    if (byte_count >= kGiB) {
        swprintf_s(text, L"%.1f GB", static_cast<double>(byte_count) / static_cast<double>(kGiB));
    } else if (byte_count >= kMiB) {
        swprintf_s(text, L"%.1f MB", static_cast<double>(byte_count) / static_cast<double>(kMiB));
    } else if (byte_count >= kKiB) {
        swprintf_s(text, L"%.1f KB", static_cast<double>(byte_count) / static_cast<double>(kKiB));
    } else {
        swprintf_s(text, L"%llu bytes", byte_count);
    }
    return text;
}

std::wstring FormatFileTime(FILETIME file_time)
{
    FILETIME local_time = {};
    SYSTEMTIME system_time = {};
    if (!FileTimeToLocalFileTime(&file_time, &local_time) || !FileTimeToSystemTime(&local_time, &system_time)) {
        return ImgViewerString(ImgViewerStringId::Unavailable);
    }

    wchar_t date_text[64] = {};
    wchar_t time_text[64] = {};
    if (GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &system_time, nullptr, date_text, ARRAYSIZE(date_text), nullptr) == 0 ||
        GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &system_time, nullptr, time_text, ARRAYSIZE(time_text)) == 0) {
        return ImgViewerString(ImgViewerStringId::Unavailable);
    }

    return std::wstring(date_text) + L" " + time_text;
}

std::wstring FormatImageDimensions(D2D1_SIZE_U size)
{
    if (size.width == 0 || size.height == 0) {
        return L"-";
    }

    wchar_t text[64] = {};
    swprintf_s(text, L"%ux%u", size.width, size.height);
    return text;
}

std::wstring FormatImageType(const std::wstring& path, bool clipboard)
{
    if (clipboard) {
        return ImgViewerString(ImgViewerStringId::ClipboardImage);
    }

    std::wstring extension = std::filesystem::path(path).extension().wstring();
    if (extension.empty()) {
        return ImgViewerString(ImgViewerStringId::Unavailable);
    }
    if (extension[0] == L'.') {
        extension.erase(extension.begin());
    }
    std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t value) {
        return static_cast<wchar_t>(std::towupper(value));
    });
    return extension;
}

} // namespace util
