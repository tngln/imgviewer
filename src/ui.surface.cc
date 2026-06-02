#include "ui.surface.hpp"

#include <wil/result_macros.h>

namespace {

struct SurfaceLayerDefinition final {
    UiSurfaceLayerId id;
    DXGI_ALPHA_MODE alpha_mode;
};

constexpr SurfaceLayerDefinition kSurfaceLayerDefinitions[] = {
    {UiSurfaceLayerId::Image, DXGI_ALPHA_MODE_IGNORE},
    {UiSurfaceLayerId::UiOverlay, DXGI_ALPHA_MODE_PREMULTIPLIED},
};

constexpr size_t LayerIndex(UiSurfaceLayerId id)
{
    return static_cast<size_t>(id);
}

const SurfaceLayerDefinition* FindLayerDefinition(UiSurfaceLayerId id)
{
    for (const SurfaceLayerDefinition& definition : kSurfaceLayerDefinitions) {
        if (definition.id == id) {
            return &definition;
        }
    }

    return nullptr;
}

} // namespace

HRESULT UiSurfaceManager::Initialize(IDCompositionDevice* device, IDCompositionVisual* root_visual)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, device);
    RETURN_HR_IF_NULL(E_INVALIDARG, root_visual);

    device_ = device;
    root_visual_ = root_visual;

    IDCompositionVisual* previous_visual = nullptr;
    for (const SurfaceLayerDefinition& definition : kSurfaceLayerDefinitions) {
        Layer& layer = LayerFor(definition.id);
        RETURN_IF_FAILED(device_->CreateVisual(layer.visual.put()));
        RETURN_IF_FAILED(root_visual_->AddVisual(layer.visual.get(), previous_visual != nullptr, previous_visual));
        previous_visual = layer.visual.get();
    }

    return S_OK;
}

HRESULT UiSurfaceManager::Resize(UINT width, UINT height)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, device_);

    width_ = width;
    height_ = height;
    for (const SurfaceLayerDefinition& definition : kSurfaceLayerDefinitions) {
        RETURN_IF_FAILED(CreateLayerSurface(definition.id, width, height));
    }

    return S_OK;
}

IDCompositionSurface* UiSurfaceManager::Surface(UiSurfaceLayerId id) const
{
    return LayerFor(id).surface.get();
}

DXGI_ALPHA_MODE UiSurfaceManager::AlphaMode(UiSurfaceLayerId id) const
{
    const SurfaceLayerDefinition* definition = FindLayerDefinition(id);
    return definition != nullptr ? definition->alpha_mode : DXGI_ALPHA_MODE_IGNORE;
}

UiSurfaceManager::Layer& UiSurfaceManager::LayerFor(UiSurfaceLayerId id)
{
    FAIL_FAST_IF(LayerIndex(id) >= layers_.size());
    return layers_[LayerIndex(id)];
}

const UiSurfaceManager::Layer& UiSurfaceManager::LayerFor(UiSurfaceLayerId id) const
{
    FAIL_FAST_IF(LayerIndex(id) >= layers_.size());
    return layers_[LayerIndex(id)];
}

HRESULT UiSurfaceManager::CreateLayerSurface(UiSurfaceLayerId id, UINT width, UINT height)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, device_);
    Layer& layer = LayerFor(id);
    RETURN_HR_IF_NULL(E_UNEXPECTED, layer.visual);

    if (layer.surface && layer.allocated_width == width && layer.allocated_height == height) {
        return S_OK;
    }

    layer.surface.reset();
    RETURN_IF_FAILED(device_->CreateSurface(
        width,
        height,
        DXGI_FORMAT_B8G8R8A8_UNORM,
        AlphaMode(id),
        layer.surface.put()));

    RETURN_IF_FAILED(layer.visual->SetContent(layer.surface.get()));
    RETURN_IF_FAILED(layer.visual->SetOffsetX(0.0f));
    RETURN_IF_FAILED(layer.visual->SetOffsetY(0.0f));
    layer.allocated_width = width;
    layer.allocated_height = height;

    return S_OK;
}
