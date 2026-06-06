#include "image.decoder.hpp"

#include <algorithm>
#include <cwctype>
#include <filesystem>

#include <wil/result_macros.h>

#include "image.stb_decoder.hpp"

namespace {

bool IsStbFallbackExtension(const wchar_t* path)
{
    if (path == nullptr) {
        return false;
    }

    std::wstring extension = std::filesystem::path(path).extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t value) {
        return static_cast<wchar_t>(std::towlower(value));
    });
    return extension == L".tga" || extension == L".psd";
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
