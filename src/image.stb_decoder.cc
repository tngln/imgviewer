#include "image.stb_decoder.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <vector>

#include <wil/result_macros.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_TGA
#define STBI_ONLY_PSD
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244 4365 4456 4457 4505 4668 5039)
#endif
#include "stb_image.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace {

struct StbImageDeleter final {
    void operator()(void* pixels) const
    {
        stbi_image_free(pixels);
    }
};

using StbImagePixels = std::unique_ptr<stbi_uc, StbImageDeleter>;

HRESULT ReadFileBytes(const wchar_t* path, std::vector<stbi_uc>* bytes)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, path);
    RETURN_HR_IF_NULL(E_POINTER, bytes);

    std::ifstream stream(std::filesystem::path(path), std::ios::binary | std::ios::ate);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), !stream);

    const std::streamoff file_size = stream.tellg();
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), file_size < 0);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE), file_size > (std::numeric_limits<int>::max)());

    bytes->resize(static_cast<size_t>(file_size));
    stream.seekg(0, std::ios::beg);
    if (!bytes->empty()) {
        stream.read(reinterpret_cast<char*>(bytes->data()), static_cast<std::streamsize>(bytes->size()));
        RETURN_HR_IF(E_FAIL, stream.gcount() != static_cast<std::streamsize>(bytes->size()));
    }

    return S_OK;
}

HRESULT ConvertRgbaToBgra(const stbi_uc* rgba, int width, int height, std::vector<BYTE>* bgra)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, rgba);
    RETURN_HR_IF_NULL(E_POINTER, bgra);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), width <= 0 || height <= 0);

    const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
    RETURN_HR_IF(
        HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW),
        pixel_count > (std::numeric_limits<size_t>::max)() / 4);

    bgra->resize(pixel_count * 4);
    for (size_t index = 0; index < pixel_count; ++index) {
        const size_t offset = index * 4;
        (*bgra)[offset] = rgba[offset + 2];
        (*bgra)[offset + 1] = rgba[offset + 1];
        (*bgra)[offset + 2] = rgba[offset];
        (*bgra)[offset + 3] = rgba[offset + 3];
    }

    return S_OK;
}

} // namespace

HRESULT DecodeStbImageFile(
    IWICImagingFactory2* wic_factory,
    const wchar_t* path,
    IWICBitmapSource** source)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, wic_factory);
    RETURN_HR_IF_NULL(E_INVALIDARG, path);
    RETURN_HR_IF_NULL(E_POINTER, source);
    *source = nullptr;

    std::vector<stbi_uc> bytes;
    RETURN_IF_FAILED(ReadFileBytes(path, &bytes));
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), bytes.empty());

    int width = 0;
    int height = 0;
    int channels = 0;
    StbImagePixels rgba(stbi_load_from_memory(
        bytes.data(),
        static_cast<int>(bytes.size()),
        &width,
        &height,
        &channels,
        4));
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), !rgba);

    std::vector<BYTE> bgra;
    RETURN_IF_FAILED(ConvertRgbaToBgra(rgba.get(), width, height, &bgra));

    const size_t stride_size = static_cast<size_t>(width) * 4;
    RETURN_HR_IF(
        HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW),
        stride_size > (std::numeric_limits<UINT>::max)() ||
            bgra.size() > (std::numeric_limits<UINT>::max)());

    wil::com_ptr<IWICBitmap> bitmap;
    RETURN_IF_FAILED(wic_factory->CreateBitmapFromMemory(
        static_cast<UINT>(width),
        static_cast<UINT>(height),
        GUID_WICPixelFormat32bppBGRA,
        static_cast<UINT>(stride_size),
        static_cast<UINT>(bgra.size()),
        bgra.data(),
        bitmap.put()));

    wil::com_ptr<IWICBitmap> cached_bitmap;
    RETURN_IF_FAILED(wic_factory->CreateBitmapFromSource(
        bitmap.get(),
        WICBitmapCacheOnLoad,
        cached_bitmap.put()));

    *source = cached_bitmap.detach();
    return S_OK;
}
