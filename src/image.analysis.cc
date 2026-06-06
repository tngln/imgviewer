#include "image.analysis.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include <wil/result_macros.h>

namespace {

constexpr unsigned int kTargetSamplePixels = 1000000;
constexpr unsigned int kBytesPerPixel = 4;

unsigned int Luma(BYTE red, BYTE green, BYTE blue)
{
    return (static_cast<unsigned int>(red) * 299U +
               static_cast<unsigned int>(green) * 587U +
               static_cast<unsigned int>(blue) * 114U +
               500U) /
        1000U;
}

unsigned int SamplingStep(UINT width, UINT height)
{
    if (width == 0 || height == 0) {
        return 1;
    }

    const uint64_t pixel_count = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
    if (pixel_count <= kTargetSamplePixels) {
        return 1;
    }

    const double ratio = static_cast<double>(pixel_count) / static_cast<double>(kTargetSamplePixels);
    return (std::max)(1U, static_cast<unsigned int>(std::ceil(std::sqrt(ratio))));
}

BYTE AverageChannel(uint64_t sum, unsigned int count)
{
    if (count == 0) {
        return 0;
    }

    return static_cast<BYTE>((std::min<uint64_t>)(255ULL, (sum + count / 2ULL) / count));
}

} // namespace

HRESULT AnalyzeImagePixels(IWICBitmapSource* source, ImagePixelAnalysis* analysis)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, source);
    RETURN_HR_IF_NULL(E_POINTER, analysis);

    UINT width = 0;
    UINT height = 0;
    RETURN_IF_FAILED(source->GetSize(&width, &height));
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), width == 0 || height == 0);
    RETURN_HR_IF(
        HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW),
        width > (std::numeric_limits<UINT>::max)() / kBytesPerPixel);

    ImagePixelAnalysis result;
    const unsigned int step = SamplingStep(width, height);
    result.downsampled = step > 1;

    const UINT stride = width * kBytesPerPixel;
    std::vector<BYTE> row(stride);
    uint64_t red_sum = 0;
    uint64_t green_sum = 0;
    uint64_t blue_sum = 0;
    unsigned int darkest_luma = 256;
    unsigned int brightest_luma = 0;

    for (UINT y = 0; y < height; y += step) {
        const WICRect rect{
            0,
            static_cast<INT>(y),
            static_cast<INT>(width),
            1,
        };
        RETURN_IF_FAILED(source->CopyPixels(&rect, stride, static_cast<UINT>(row.size()), row.data()));

        for (UINT x = 0; x < width; x += step) {
            const size_t offset = static_cast<size_t>(x) * kBytesPerPixel;
            const BYTE blue = row[offset];
            const BYTE green = row[offset + 1];
            const BYTE red = row[offset + 2];
            const unsigned int luma = Luma(red, green, blue);

            ++result.luma_histogram[luma];
            ++result.red_histogram[red];
            ++result.green_histogram[green];
            ++result.blue_histogram[blue];

            red_sum += red;
            green_sum += green;
            blue_sum += blue;
            ++result.sampled_pixels;

            if (luma < darkest_luma) {
                darkest_luma = luma;
                result.darkest = ImageColorSample{.red = red, .green = green, .blue = blue};
            }
            if (luma >= brightest_luma) {
                brightest_luma = luma;
                result.brightest = ImageColorSample{.red = red, .green = green, .blue = blue};
            }
        }
    }

    result.average = ImageColorSample{
        .red = AverageChannel(red_sum, result.sampled_pixels),
        .green = AverageChannel(green_sum, result.sampled_pixels),
        .blue = AverageChannel(blue_sum, result.sampled_pixels),
    };

    *analysis = result;
    return S_OK;
}
