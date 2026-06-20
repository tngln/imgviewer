#include "image.bitmap.hpp"

#include <limits>

#include <wil/com.h>
#include <wil/result_macros.h>

namespace image_bitmap {

HRESULT CreateBitmapFromMemory(
    IWICImagingFactory2* wic_factory,
    UINT width,
    UINT height,
    REFWICPixelFormatGUID pixel_format,
    UINT stride,
    const BYTE* pixels,
    size_t pixel_size,
    IWICBitmap** bitmap)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, wic_factory);
    RETURN_HR_IF_NULL(E_POINTER, bitmap);
    *bitmap = nullptr;
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), width == 0 || height == 0);

    RETURN_HR_IF(
        HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW),
        stride > (std::numeric_limits<UINT>::max)() ||
            pixel_size > (std::numeric_limits<UINT>::max)());

    wil::com_ptr<IWICBitmap> memory_bitmap;
    RETURN_IF_FAILED(wic_factory->CreateBitmapFromMemory(
        width,
        height,
        pixel_format,
        stride,
        static_cast<UINT>(pixel_size),
        const_cast<BYTE*>(pixels),
        memory_bitmap.put()));

    *bitmap = memory_bitmap.detach();
    return S_OK;
}

HRESULT CreateCachedBitmapFromSource(
    IWICImagingFactory2* wic_factory,
    IWICBitmapSource* source,
    IWICBitmap** cached)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, wic_factory);
    RETURN_HR_IF_NULL(E_INVALIDARG, source);
    RETURN_HR_IF_NULL(E_POINTER, cached);
    *cached = nullptr;

    wil::com_ptr<IWICBitmap> cached_bitmap;
    RETURN_IF_FAILED(wic_factory->CreateBitmapFromSource(
        source,
        WICBitmapCacheOnLoad,
        cached_bitmap.put()));

    *cached = cached_bitmap.detach();
    return S_OK;
}

HRESULT CreateBitmapSourceFromBgra(
    IWICImagingFactory2* wic_factory,
    UINT width,
    UINT height,
    const BYTE* bgra,
    size_t bgra_size,
    IWICBitmapSource** source)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, wic_factory);
    RETURN_HR_IF_NULL(E_POINTER, source);
    *source = nullptr;
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), width == 0 || height == 0);

    const size_t stride = static_cast<size_t>(width) * 4;
    RETURN_HR_IF(
        HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW),
        stride > (std::numeric_limits<size_t>::max)() / height);
    RETURN_HR_IF(
        HRESULT_FROM_WIN32(ERROR_INVALID_DATA),
        bgra_size != stride * static_cast<size_t>(height));

    wil::com_ptr<IWICBitmap> memory_bitmap;
    RETURN_IF_FAILED(CreateBitmapFromMemory(
        wic_factory,
        width,
        height,
        GUID_WICPixelFormat32bppBGRA,
        static_cast<UINT>(stride),
        bgra,
        bgra_size,
        memory_bitmap.put()));

    wil::com_ptr<IWICBitmap> cached_bitmap;
    RETURN_IF_FAILED(CreateCachedBitmapFromSource(wic_factory, memory_bitmap.get(), cached_bitmap.put()));

    *source = cached_bitmap.detach();
    return S_OK;
}

HRESULT CreateBitmapSourceFromBgra(
    IWICImagingFactory2* wic_factory,
    UINT width,
    UINT height,
    const std::vector<BYTE>& bgra,
    IWICBitmapSource** source)
{
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), bgra.empty());
    return CreateBitmapSourceFromBgra(wic_factory, width, height, bgra.data(), bgra.size(), source);
}

HRESULT CopySourceBgra(
    IWICImagingFactory2* wic_factory,
    IWICBitmapSource* source,
    std::vector<BYTE>* bgra,
    UINT* width,
    UINT* height)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, wic_factory);
    RETURN_HR_IF_NULL(E_INVALIDARG, source);
    RETURN_HR_IF_NULL(E_POINTER, bgra);
    RETURN_HR_IF_NULL(E_POINTER, width);
    RETURN_HR_IF_NULL(E_POINTER, height);

    wil::com_ptr<IWICFormatConverter> converter;
    RETURN_IF_FAILED(wic_factory->CreateFormatConverter(converter.put()));
    RETURN_IF_FAILED(converter->Initialize(
        source,
        GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeMedianCut));

    RETURN_IF_FAILED(converter->GetSize(width, height));
    const size_t stride = static_cast<size_t>(*width) * 4;
    const size_t byte_count = stride * static_cast<size_t>(*height);
    RETURN_HR_IF(
        HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW),
        stride > (std::numeric_limits<UINT>::max)() ||
            byte_count > (std::numeric_limits<UINT>::max)());

    bgra->resize(byte_count);
    RETURN_IF_FAILED(converter->CopyPixels(
        nullptr,
        static_cast<UINT>(stride),
        static_cast<UINT>(bgra->size()),
        bgra->data()));
    return S_OK;
}

} // namespace image_bitmap
