#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

#include <windows.h>

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

// Reads up to max_size bytes from the start of path. If the file is larger
// than max_size, only the first max_size bytes are returned (no error).
// max_size defaults to DWORD_MAX which is the practical upper bound for WIC.
HRESULT ReadFileBytes(
    const wchar_t* path,
    std::vector<BYTE>* bytes,
    size_t max_size = static_cast<size_t>((std::numeric_limits<DWORD>::max)()));

inline uint16_t ReadBe16(const BYTE* data)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
}

inline uint32_t ReadBe32(const BYTE* data)
{
    return (static_cast<uint32_t>(data[0]) << 24) |
        (static_cast<uint32_t>(data[1]) << 16) |
        (static_cast<uint32_t>(data[2]) << 8) |
        static_cast<uint32_t>(data[3]);
}

inline uint32_t ReadLe24(const BYTE* data)
{
    return static_cast<uint32_t>(data[0]) |
        (static_cast<uint32_t>(data[1]) << 8) |
        (static_cast<uint32_t>(data[2]) << 16);
}

inline uint32_t ReadLe32(const BYTE* data)
{
    return static_cast<uint32_t>(data[0]) |
        (static_cast<uint32_t>(data[1]) << 8) |
        (static_cast<uint32_t>(data[2]) << 16) |
        (static_cast<uint32_t>(data[3]) << 24);
}

inline void AppendBe32(std::vector<BYTE>* bytes, uint32_t value)
{
    bytes->push_back(static_cast<BYTE>((value >> 24) & 0xff));
    bytes->push_back(static_cast<BYTE>((value >> 16) & 0xff));
    bytes->push_back(static_cast<BYTE>((value >> 8) & 0xff));
    bytes->push_back(static_cast<BYTE>(value & 0xff));
}

inline void AppendLe32(std::vector<BYTE>* bytes, uint32_t value)
{
    bytes->push_back(static_cast<BYTE>(value & 0xff));
    bytes->push_back(static_cast<BYTE>((value >> 8) & 0xff));
    bytes->push_back(static_cast<BYTE>((value >> 16) & 0xff));
    bytes->push_back(static_cast<BYTE>((value >> 24) & 0xff));
}

template <typename Bytes>
inline bool IsFourCC(const Bytes* data, const char (&fourcc)[5])
{
    return static_cast<BYTE>(data[0]) == static_cast<BYTE>(fourcc[0]) &&
        static_cast<BYTE>(data[1]) == static_cast<BYTE>(fourcc[1]) &&
        static_cast<BYTE>(data[2]) == static_cast<BYTE>(fourcc[2]) &&
        static_cast<BYTE>(data[3]) == static_cast<BYTE>(fourcc[3]);
}

} // namespace image_utils
