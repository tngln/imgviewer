#pragma once

#include <windows.h>
#include <wincodec.h>

class ImageEncoder final {
public:
    HRESULT Initialize(IWICImagingFactory2* wic_factory);
    HRESULT SavePngFile(IWICBitmapSource* source, const wchar_t* path);

private:
    IWICImagingFactory2* wic_factory_ = nullptr;
};
