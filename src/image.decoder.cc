#include "image.decoder.hpp"

#include <wil/result_macros.h>

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
    RETURN_IF_FAILED(wic_factory_->CreateDecoderFromFilename(
        path,
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        decoder.put()));

    wil::com_ptr<IWICBitmapFrameDecode> frame;
    RETURN_IF_FAILED(decoder->GetFrame(0, frame.put()));
    RETURN_IF_FAILED(frame->GetSize(&decoded.pixel_size.width, &decoded.pixel_size.height));

    wil::com_ptr<IWICFormatConverter> bgra_converter;
    RETURN_IF_FAILED(wic_factory_->CreateFormatConverter(bgra_converter.put()));
    RETURN_IF_FAILED(bgra_converter->Initialize(
        frame.get(),
        GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeMedianCut));
    decoded.pixel_source = bgra_converter;

    wil::com_ptr<IWICFormatConverter> pbgra_converter;
    RETURN_IF_FAILED(wic_factory_->CreateFormatConverter(pbgra_converter.put()));
    RETURN_IF_FAILED(pbgra_converter->Initialize(
        frame.get(),
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
