#pragma once

#include <d2d1_1.h>
#include <wincodec.h>

#include <wil/com.h>

struct DecodedImage final {
    wil::com_ptr<ID2D1Bitmap1> bitmap;
    D2D1_SIZE_U pixel_size = {};
};

class ImageDecoder final {
public:
    HRESULT Initialize();
    HRESULT DecodeFirstFrame(
        const wchar_t* path,
        ID2D1DeviceContext* d2d_context,
        DecodedImage* image);

private:
    wil::com_ptr<IWICImagingFactory2> wic_factory_;
};
