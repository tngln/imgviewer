#include "ui.graphics_device.hpp"

#include <algorithm>

#include <d2d1helper.h>
#include <wil/result_macros.h>

HRESULT GraphicsDevice::Initialize()
{
    if (d2d_context_ != nullptr) {
        return S_OK;
    }

    D2D1_FACTORY_OPTIONS factory_options = {};
#if defined(_DEBUG)
    factory_options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

    RETURN_IF_FAILED(D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        factory_options,
        d2d_factory_.put()));

    constexpr UINT device_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    constexpr D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    RETURN_IF_FAILED(D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        device_flags,
        feature_levels,
        ARRAYSIZE(feature_levels),
        D3D11_SDK_VERSION,
        d3d_device_.put(),
        nullptr,
        d3d_context_.put()));

    RETURN_IF_FAILED(d3d_device_->QueryInterface(IID_PPV_ARGS(dxgi_device_.put())));
    RETURN_IF_FAILED(d2d_factory_->CreateDevice(dxgi_device_.get(), d2d_device_.put()));
    RETURN_IF_FAILED(d2d_device_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2d_context_.put()));
    RETURN_IF_FAILED(DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(dwrite_factory_.put())));
    RETURN_IF_FAILED(DCompositionCreateDevice(
        dxgi_device_.get(),
        __uuidof(IDCompositionDevice),
        reinterpret_cast<void**>(dcomp_device_.put())));
    return S_OK;
}

ID2D1Factory1* GraphicsDevice::D2DFactory() const
{
    return d2d_factory_.get();
}

ID3D11Device* GraphicsDevice::D3DDevice() const
{
    return d3d_device_.get();
}

ID3D11DeviceContext* GraphicsDevice::D3DContext() const
{
    return d3d_context_.get();
}

IDXGIDevice* GraphicsDevice::DxgiDevice() const
{
    return dxgi_device_.get();
}

ID2D1Device* GraphicsDevice::D2DDevice() const
{
    return d2d_device_.get();
}

ID2D1DeviceContext* GraphicsDevice::D2DContext() const
{
    return d2d_context_.get();
}

IDWriteFactory* GraphicsDevice::DWriteFactory() const
{
    return dwrite_factory_.get();
}

IDCompositionDevice* GraphicsDevice::DCompDevice() const
{
    return dcomp_device_.get();
}

HRESULT GraphicsDevice::CreateCompositionTarget(
    HWND hwnd,
    IDCompositionTarget** target,
    IDCompositionVisual** root_visual) const
{
    RETURN_HR_IF_NULL(E_INVALIDARG, hwnd);
    RETURN_HR_IF_NULL(E_POINTER, target);
    RETURN_HR_IF_NULL(E_POINTER, root_visual);
    RETURN_HR_IF_NULL(E_UNEXPECTED, dcomp_device_);

    wil::com_ptr<IDCompositionTarget> local_target;
    wil::com_ptr<IDCompositionVisual> local_root;
    RETURN_IF_FAILED(dcomp_device_->CreateTargetForHwnd(hwnd, TRUE, local_target.put()));
    RETURN_IF_FAILED(dcomp_device_->CreateVisual(local_root.put()));
    RETURN_IF_FAILED(local_target->SetRoot(local_root.get()));
    *target = local_target.detach();
    *root_visual = local_root.detach();
    return dcomp_device_->Commit();
}

HRESULT GraphicsDevice::CreateTargetBitmapFromDxgiSurface(
    IDXGISurface* surface,
    DXGI_FORMAT format,
    DXGI_ALPHA_MODE alpha_mode,
    ID2D1Bitmap1** bitmap) const
{
    RETURN_HR_IF_NULL(E_INVALIDARG, surface);
    RETURN_HR_IF_NULL(E_POINTER, bitmap);
    RETURN_HR_IF_NULL(E_UNEXPECTED, d2d_context_);

    const D2D1_BITMAP_PROPERTIES1 bitmap_properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(format, static_cast<D2D1_ALPHA_MODE>(alpha_mode)),
        96.0f,
        96.0f);
    return d2d_context_->CreateBitmapFromDxgiSurface(surface, bitmap_properties, bitmap);
}

HRESULT GraphicsDevice::RenderTextureToWicBitmap(
    IWICImagingFactory2* wic_factory,
    UINT width,
    UINT height,
    GraphicsDeviceDrawCallback callback,
    void* user_data,
    IWICBitmapSource** source) const
{
    RETURN_HR_IF_NULL(E_INVALIDARG, wic_factory);
    RETURN_HR_IF_NULL(E_INVALIDARG, callback);
    RETURN_HR_IF_NULL(E_POINTER, source);
    RETURN_HR_IF_NULL(E_UNEXPECTED, d3d_device_);
    RETURN_HR_IF_NULL(E_UNEXPECTED, d3d_context_);
    RETURN_HR_IF_NULL(E_UNEXPECTED, d2d_context_);
    *source = nullptr;

    width = (std::max)(1U, width);
    height = (std::max)(1U, height);

    D3D11_TEXTURE2D_DESC target_desc = {};
    target_desc.Width = width;
    target_desc.Height = height;
    target_desc.MipLevels = 1;
    target_desc.ArraySize = 1;
    target_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    target_desc.SampleDesc.Count = 1;
    target_desc.Usage = D3D11_USAGE_DEFAULT;
    target_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    wil::com_ptr<ID3D11Texture2D> target_texture;
    RETURN_IF_FAILED(d3d_device_->CreateTexture2D(&target_desc, nullptr, target_texture.put()));

    wil::com_ptr<IDXGISurface> target_surface;
    RETURN_IF_FAILED(target_texture->QueryInterface(IID_PPV_ARGS(target_surface.put())));

    wil::com_ptr<ID2D1Bitmap1> target_bitmap;
    RETURN_IF_FAILED(CreateTargetBitmapFromDxgiSurface(
        target_surface.get(),
        DXGI_FORMAT_B8G8R8A8_UNORM,
        DXGI_ALPHA_MODE_PREMULTIPLIED,
        target_bitmap.put()));

    d2d_context_->SetTarget(target_bitmap.get());
    d2d_context_->BeginDraw();
    const HRESULT callback_result = callback(d2d_context_.get(), user_data);
    const HRESULT end_draw_result = d2d_context_->EndDraw();
    d2d_context_->SetTarget(nullptr);
    RETURN_IF_FAILED(callback_result);
    RETURN_IF_FAILED(end_draw_result);

    D3D11_TEXTURE2D_DESC staging_desc = target_desc;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.BindFlags = 0;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    wil::com_ptr<ID3D11Texture2D> staging_texture;
    RETURN_IF_FAILED(d3d_device_->CreateTexture2D(&staging_desc, nullptr, staging_texture.put()));
    d3d_context_->CopyResource(staging_texture.get(), target_texture.get());

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    RETURN_IF_FAILED(d3d_context_->Map(staging_texture.get(), 0, D3D11_MAP_READ, 0, &mapped));

    wil::com_ptr<IWICBitmap> bitmap;
    HRESULT hr = wic_factory->CreateBitmap(
        width,
        height,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapCacheOnLoad,
        bitmap.put());
    if (SUCCEEDED(hr)) {
        WICRect rect{0, 0, static_cast<INT>(width), static_cast<INT>(height)};
        wil::com_ptr<IWICBitmapLock> lock;
        hr = bitmap->Lock(&rect, WICBitmapLockWrite, lock.put());
        if (SUCCEEDED(hr)) {
            UINT destination_size = 0;
            BYTE* destination = nullptr;
            UINT destination_stride = 0;
            hr = lock->GetDataPointer(&destination_size, &destination);
            if (SUCCEEDED(hr)) {
                hr = lock->GetStride(&destination_stride);
            }
            if (SUCCEEDED(hr)) {
                const auto* source_bytes = static_cast<const BYTE*>(mapped.pData);
                for (UINT y = 0; y < height; ++y) {
                    CopyMemory(
                        destination + static_cast<size_t>(destination_stride) * y,
                        source_bytes + static_cast<size_t>(mapped.RowPitch) * y,
                        static_cast<size_t>(width) * 4);
                }
            }
        }
    }

    d3d_context_->Unmap(staging_texture.get(), 0);
    RETURN_IF_FAILED(hr);
    *source = bitmap.detach();
    return S_OK;
}
