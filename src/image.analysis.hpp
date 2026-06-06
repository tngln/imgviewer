#pragma once

#include <array>

#include <windows.h>
#include <wincodec.h>

struct ImageColorSample final {
    BYTE red = 0;
    BYTE green = 0;
    BYTE blue = 0;
};

struct ImagePixelAnalysis final {
    std::array<unsigned int, 256> luma_histogram = {};
    std::array<unsigned int, 256> red_histogram = {};
    std::array<unsigned int, 256> green_histogram = {};
    std::array<unsigned int, 256> blue_histogram = {};
    ImageColorSample average;
    ImageColorSample darkest;
    ImageColorSample brightest;
    unsigned int sampled_pixels = 0;
    bool downsampled = false;
};

HRESULT AnalyzeImagePixels(IWICBitmapSource* source, ImagePixelAnalysis* analysis);
