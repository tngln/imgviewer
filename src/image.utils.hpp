#pragma once

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <string>

namespace image_utils {

inline std::wstring ToLowerExtension(const wchar_t* path)
{
    if (path == nullptr) {
        return {};
    }

    std::wstring ext = std::filesystem::path(path).extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towlower(c));
    });
    return ext;
}

inline bool IsImageExtension(const wchar_t* path)
{
    const std::wstring ext = ToLowerExtension(path);
    return ext == L".bmp" ||
        ext == L".dib" ||
        ext == L".gif" ||
        ext == L".ico" ||
        ext == L".jpg" ||
        ext == L".jpeg" ||
        ext == L".jpe" ||
        ext == L".png" ||
        ext == L".psd" ||
        ext == L".tif" ||
        ext == L".tiff" ||
        ext == L".tga" ||
        ext == L".webp";
}

} // namespace image_utils
