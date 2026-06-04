#include "image.encoder.hpp"

#include <wil/com.h>
#include <wil/result_macros.h>

HRESULT ImageEncoder::Initialize(IWICImagingFactory2* wic_factory)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, wic_factory);
    wic_factory_ = wic_factory;
    return S_OK;
}

HRESULT ImageEncoder::SavePngFile(IWICBitmapSource* source, const wchar_t* path)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, source);
    RETURN_HR_IF_NULL(E_INVALIDARG, path);
    RETURN_HR_IF(E_INVALIDARG, path[0] == L'\0');
    RETURN_HR_IF_NULL(E_UNEXPECTED, wic_factory_);

    UINT width = 0;
    UINT height = 0;
    RETURN_IF_FAILED(source->GetSize(&width, &height));

    wil::com_ptr<IWICStream> stream;
    RETURN_IF_FAILED(wic_factory_->CreateStream(stream.put()));
    RETURN_IF_FAILED(stream->InitializeFromFilename(path, GENERIC_WRITE));

    wil::com_ptr<IWICBitmapEncoder> encoder;
    RETURN_IF_FAILED(wic_factory_->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.put()));
    RETURN_IF_FAILED(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache));

    wil::com_ptr<IWICBitmapFrameEncode> frame;
    wil::com_ptr<IPropertyBag2> properties;
    RETURN_IF_FAILED(encoder->CreateNewFrame(frame.put(), properties.put()));
    RETURN_IF_FAILED(frame->Initialize(properties.get()));
    RETURN_IF_FAILED(frame->SetSize(width, height));

    WICPixelFormatGUID pixel_format = GUID_WICPixelFormat32bppBGRA;
    RETURN_IF_FAILED(frame->SetPixelFormat(&pixel_format));
    RETURN_IF_FAILED(frame->WriteSource(source, nullptr));
    RETURN_IF_FAILED(frame->Commit());
    RETURN_IF_FAILED(encoder->Commit());
    return S_OK;
}
