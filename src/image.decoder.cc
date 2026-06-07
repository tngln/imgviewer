#include "image.decoder.hpp"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <limits>

#include <wil/result_macros.h>

#include "image.animation_decoder.hpp"
#include "image.stb_decoder.hpp"
#include "image.utils.hpp"

namespace {

bool IsStbFallbackExtension(const wchar_t* path)
{
    const std::wstring ext = image_utils::ToLowerExtension(path);
    return ext == L".tga" || ext == L".psd";
}

HRESULT CreateBitmapSourceFromBgra(
    IWICImagingFactory2* wic_factory,
    UINT width,
    UINT height,
    const std::vector<BYTE>& bgra,
    IWICBitmapSource** source)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, wic_factory);
    RETURN_HR_IF_NULL(E_POINTER, source);
    *source = nullptr;
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), width == 0 || height == 0);

    const size_t stride = static_cast<size_t>(width) * 4;
    RETURN_HR_IF(
        HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW),
        stride > (std::numeric_limits<UINT>::max)() ||
            bgra.size() > (std::numeric_limits<UINT>::max)());
    RETURN_HR_IF(
        HRESULT_FROM_WIN32(ERROR_INVALID_DATA),
        bgra.size() != stride * static_cast<size_t>(height));

    wil::com_ptr<IWICBitmap> bitmap;
    RETURN_IF_FAILED(wic_factory->CreateBitmapFromMemory(
        width,
        height,
        GUID_WICPixelFormat32bppBGRA,
        static_cast<UINT>(stride),
        static_cast<UINT>(bgra.size()),
        const_cast<BYTE*>(bgra.data()),
        bitmap.put()));

    wil::com_ptr<IWICBitmap> cached_bitmap;
    RETURN_IF_FAILED(wic_factory->CreateBitmapFromSource(
        bitmap.get(),
        WICBitmapCacheOnLoad,
        cached_bitmap.put()));

    *source = cached_bitmap.detach();
    return S_OK;
}

} // namespace

HRESULT ImageDecoder::Initialize()
{
    RETURN_IF_FAILED(CoCreateInstance(
        CLSID_WICImagingFactory2,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(wic_factory_.put())));

    return S_OK;
}

HRESULT ImageDecoder::DecodeImageFile(
    const wchar_t* path,
    ID2D1DeviceContext* d2d_context,
    DecodedImageSet* image_set)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, path);
    RETURN_HR_IF_NULL(E_INVALIDARG, d2d_context);
    RETURN_HR_IF_NULL(E_POINTER, image_set);
    RETURN_HR_IF_NULL(E_UNEXPECTED, wic_factory_);

    DecodedImageSet decoded_set;
    wil::com_ptr<IWICBitmapDecoder> decoder;
    const HRESULT wic_hr = wic_factory_->CreateDecoderFromFilename(
        path,
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        decoder.put());

    if (SUCCEEDED(wic_hr)) {
        AnimationPixels animation;
        const std::wstring ext = image_utils::ToLowerExtension(path);
        HRESULT animation_hr = HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        if (ext == L".gif") {
            animation_hr = DecodeGifAnimationPixels(wic_factory_.get(), decoder.get(), &animation);
        } else if (ext == L".png" || ext == L".apng") {
            animation_hr = DecodeApngAnimationPixels(wic_factory_.get(), path, &animation);
        } else if (ext == L".webp") {
            animation_hr = DecodeWebpAnimationPixels(wic_factory_.get(), path, &animation);
        }

        if (SUCCEEDED(animation_hr) && animation.frames.size() > 1) {
            decoded_set.animation_loop = animation.loop;
            decoded_set.animation_frames.reserve(animation.frames.size());
            for (const AnimationFramePixels& frame : animation.frames) {
                wil::com_ptr<IWICBitmapSource> source;
                RETURN_IF_FAILED(CreateBitmapSourceFromBgra(
                    wic_factory_.get(),
                    animation.width,
                    animation.height,
                    frame.bgra,
                    source.put()));

                DecodedImage decoded_frame;
                RETURN_IF_FAILED(DecodeBitmapSource(source.get(), d2d_context, &decoded_frame));
                decoded_set.animation_frames.push_back(DecodedAnimationFrame{
                    .image = std::move(decoded_frame),
                    .duration_ms = frame.duration_ms,
                });
            }

            decoded_set.image = DecodedImage{
                .bitmap = decoded_set.animation_frames.front().image.bitmap,
                .pixel_source = decoded_set.animation_frames.front().image.pixel_source,
                .pixel_size = decoded_set.animation_frames.front().image.pixel_size,
            };
            wil::com_ptr<IWICBitmapFrameDecode> first_frame;
            if (SUCCEEDED(decoder->GetFrame(0, first_frame.put()))) {
                ReadImageExifMetadata(first_frame.get(), &decoded_set.image.metadata);
            }
            *image_set = std::move(decoded_set);
            return S_OK;
        }
    }

    RETURN_IF_FAILED(DecodeFirstFrame(path, d2d_context, &decoded_set.image));
    *image_set = std::move(decoded_set);
    return S_OK;
}

HRESULT ImageDecoder::DecodeFirstFrame(
    const wchar_t* path,
    ID2D1DeviceContext* d2d_context,
    DecodedImage* image)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, path);
    RETURN_HR_IF_NULL(E_INVALIDARG, d2d_context);
    RETURN_HR_IF_NULL(E_POINTER, image);
    RETURN_HR_IF_NULL(E_UNEXPECTED, wic_factory_);

    DecodedImage decoded;
    wil::com_ptr<IWICBitmapDecoder> decoder;
    const HRESULT wic_hr = wic_factory_->CreateDecoderFromFilename(
        path,
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        decoder.put());

    if (FAILED(wic_hr)) {
        if (!IsStbFallbackExtension(path)) {
            RETURN_IF_FAILED(wic_hr);
        }

        wil::com_ptr<IWICBitmapSource> stb_source;
        RETURN_IF_FAILED(DecodeStbImageFile(wic_factory_.get(), path, stb_source.put()));
        RETURN_IF_FAILED(DecodeBitmapSource(stb_source.get(), d2d_context, &decoded));
        *image = std::move(decoded);
        return S_OK;
    }

    wil::com_ptr<IWICBitmapFrameDecode> frame;
    RETURN_IF_FAILED(decoder->GetFrame(0, frame.put()));
    RETURN_IF_FAILED(DecodeBitmapSource(frame.get(), d2d_context, &decoded));
    RETURN_IF_FAILED(ReadImageExifMetadata(frame.get(), &decoded.metadata));

    *image = std::move(decoded);
    return S_OK;
}

HRESULT ImageDecoder::DecodeBitmapSource(
    IWICBitmapSource* source,
    ID2D1DeviceContext* d2d_context,
    DecodedImage* image)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, source);
    RETURN_HR_IF_NULL(E_INVALIDARG, d2d_context);
    RETURN_HR_IF_NULL(E_POINTER, image);
    RETURN_HR_IF_NULL(E_UNEXPECTED, wic_factory_);

    DecodedImage decoded;
    RETURN_IF_FAILED(source->GetSize(&decoded.pixel_size.width, &decoded.pixel_size.height));

    wil::com_ptr<IWICFormatConverter> bgra_converter;
    RETURN_IF_FAILED(wic_factory_->CreateFormatConverter(bgra_converter.put()));
    RETURN_IF_FAILED(bgra_converter->Initialize(
        source,
        GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeMedianCut));
    decoded.pixel_source = bgra_converter;

    wil::com_ptr<IWICFormatConverter> pbgra_converter;
    RETURN_IF_FAILED(wic_factory_->CreateFormatConverter(pbgra_converter.put()));
    RETURN_IF_FAILED(pbgra_converter->Initialize(
        source,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeMedianCut));

    const D2D1_BITMAP_PROPERTIES1 bitmap_properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_NONE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f,
        96.0f);
    RETURN_IF_FAILED(d2d_context->CreateBitmapFromWicBitmap(
        pbgra_converter.get(),
        bitmap_properties,
        decoded.bitmap.put()));

    *image = std::move(decoded);
    return S_OK;
}

IWICImagingFactory2* ImageDecoder::WicFactory() const
{
    return wic_factory_.get();
}
