#pragma once

#include <windows.h>
#include <wincodec.h>

#include <wil/com.h>

HRESULT DecodeStbImageFile(
    IWICImagingFactory2* wic_factory,
    const wchar_t* path,
    IWICBitmapSource** source);
