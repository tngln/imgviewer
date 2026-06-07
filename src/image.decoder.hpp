#pragma once

#include <d2d1_1.h>
#include <wincodec.h>

#include <vector>

#include <wil/com.h>

#include "image.metadata.hpp"

struct DecodedImage final {
    wil::com_ptr<ID2D1Bitmap1> bitmap;
    wil::com_ptr<IWICBitmapSource> pixel_source;
    D2D1_SIZE_U pixel_size = {};
    ImageMetadata metadata;
};

struct DecodedAnimationFrame final {
    DecodedImage image;
    UINT duration_ms = 100;
};

struct DecodedImageSet final {
    DecodedImage image;
    std::vector<DecodedAnimationFrame> animation_frames;
    bool animation_loop = true;
};

class ImageDecoder final {
public:
    HRESULT Initialize();
    HRESULT DecodeImageFile(
        const wchar_t* path,
        ID2D1DeviceContext* d2d_context,
        DecodedImageSet* image_set);
    HRESULT DecodeFirstFrame(
        const wchar_t* path,
        ID2D1DeviceContext* d2d_context,
        DecodedImage* image);
    HRESULT DecodeBitmapSource(
        IWICBitmapSource* source,
        ID2D1DeviceContext* d2d_context,
        DecodedImage* image);
    IWICImagingFactory2* WicFactory() const;

private:
    wil::com_ptr<IWICImagingFactory2> wic_factory_;
};
