#include "surface.manager.hpp"

#include <wil/result_macros.h>

HRESULT SurfaceManager::Initialize(IDCompositionDevice* device, IDCompositionVisual* root_visual)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, device);
    RETURN_HR_IF_NULL(E_INVALIDARG, root_visual);

    device_ = device;
    root_visual_ = root_visual;
    image_layer_.alpha_mode = DXGI_ALPHA_MODE_IGNORE;
    ui_overlay_layer_.alpha_mode = DXGI_ALPHA_MODE_PREMULTIPLIED;

    RETURN_IF_FAILED(device_->CreateVisual(image_layer_.visual.put()));
    RETURN_IF_FAILED(device_->CreateVisual(ui_overlay_layer_.visual.put()));
    RETURN_IF_FAILED(root_visual_->AddVisual(image_layer_.visual.get(), FALSE, nullptr));
    RETURN_IF_FAILED(root_visual_->AddVisual(ui_overlay_layer_.visual.get(), TRUE, image_layer_.visual.get()));

    return S_OK;
}

HRESULT SurfaceManager::Resize(UINT width, UINT height)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, device_);

    width_ = width;
    height_ = height;
    RETURN_IF_FAILED(CreateLayerSurface(image_layer_, width, height));
    RETURN_IF_FAILED(CreateLayerSurface(ui_overlay_layer_, width, height));

    return S_OK;
}

IDCompositionSurface* SurfaceManager::Surface(SurfaceLayerId id) const
{
    return LayerFor(id).surface.get();
}

SurfaceManager::Layer& SurfaceManager::LayerFor(SurfaceLayerId id)
{
    return id == SurfaceLayerId::Image ? image_layer_ : ui_overlay_layer_;
}

const SurfaceManager::Layer& SurfaceManager::LayerFor(SurfaceLayerId id) const
{
    return id == SurfaceLayerId::Image ? image_layer_ : ui_overlay_layer_;
}

HRESULT SurfaceManager::CreateLayerSurface(Layer& layer, UINT width, UINT height)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, device_);
    RETURN_HR_IF_NULL(E_UNEXPECTED, layer.visual);

    if (layer.surface && layer.allocated_width >= width && layer.allocated_height >= height) {
        return S_OK;
    }

    layer.surface.reset();
    RETURN_IF_FAILED(device_->CreateSurface(
        width,
        height,
        DXGI_FORMAT_B8G8R8A8_UNORM,
        layer.alpha_mode,
        layer.surface.put()));

    RETURN_IF_FAILED(layer.visual->SetContent(layer.surface.get()));
    RETURN_IF_FAILED(layer.visual->SetOffsetX(0.0f));
    RETURN_IF_FAILED(layer.visual->SetOffsetY(0.0f));
    layer.allocated_width = width;
    layer.allocated_height = height;

    return S_OK;
}
