#pragma once

#include <windows.h>
#include <wincodec.h>

#include <vector>

struct AnimationFramePixels final {
    std::vector<BYTE> bgra;
    UINT duration_ms = 100;
};

struct AnimationPixels final {
    UINT width = 0;
    UINT height = 0;
    bool loop = true;
    std::vector<AnimationFramePixels> frames;
};

HRESULT DecodeGifAnimationPixels(
    IWICImagingFactory2* wic_factory,
    IWICBitmapDecoder* decoder,
    AnimationPixels* animation);
HRESULT DecodeApngAnimationPixels(
    IWICImagingFactory2* wic_factory,
    const wchar_t* path,
    AnimationPixels* animation);
HRESULT DecodeWebpAnimationPixels(
    IWICImagingFactory2* wic_factory,
    const wchar_t* path,
    AnimationPixels* animation);
