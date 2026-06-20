#pragma once

#include <windows.h>
#include <wincodec.h>

#include <cstddef>
#include <vector>

namespace image_bitmap {

HRESULT CreateBitmapFromMemory(
    IWICImagingFactory2* wic_factory,
    UINT width,
    UINT height,
    REFWICPixelFormatGUID pixel_format,
    UINT stride,
    const BYTE* pixels,
    size_t pixel_size,
    IWICBitmap** bitmap);

HRESULT CreateCachedBitmapFromSource(
    IWICImagingFactory2* wic_factory,
    IWICBitmapSource* source,
    IWICBitmap** cached);

HRESULT CreateBitmapSourceFromBgra(
    IWICImagingFactory2* wic_factory,
    UINT width,
    UINT height,
    const BYTE* bgra,
    size_t bgra_size,
    IWICBitmapSource** source);

HRESULT CreateBitmapSourceFromBgra(
    IWICImagingFactory2* wic_factory,
    UINT width,
    UINT height,
    const std::vector<BYTE>& bgra,
    IWICBitmapSource** source);

HRESULT CopySourceBgra(
    IWICImagingFactory2* wic_factory,
    IWICBitmapSource* source,
    std::vector<BYTE>* bgra,
    UINT* width,
    UINT* height);

} // namespace image_bitmap
