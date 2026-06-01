#include "surface.manager.hpp"

#include <wil/result_macros.h>

namespace {

struct SurfaceLayerDefinition final {
    SurfaceLayerId id;
    DXGI_ALPHA_MODE alpha_mode;
};

constexpr SurfaceLayerDefinition kSurfaceLayerDefinitions[] = {
    {SurfaceLayerId::Image, DXGI_ALPHA_MODE_IGNORE},
    {SurfaceLayerId::UiOverlay, DXGI_ALPHA_MODE_PREMULTIPLIED},
};

constexpr size_t LayerIndex(SurfaceLayerId id)
{
    return static_cast<size_t>(id);
}

const SurfaceLayerDefinition* FindLayerDefinition(SurfaceLayerId id)
{
    for (const SurfaceLayerDefinition& definition : kSurfaceLayerDefinitions) {
        if (definition.id == id) {
            return &definition;
        }
    }

    return nullptr;
}

} // namespace

HRESULT SurfaceManager::Initialize(IDCompositionDevice* device, IDCompositionVisual* root_visual)
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

HRESULT SurfaceManager::Resize(UINT width, UINT height)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, device_);

    width_ = width;
    height_ = height;
    for (const SurfaceLayerDefinition& definition : kSurfaceLayerDefinitions) {
        RETURN_IF_FAILED(CreateLayerSurface(definition.id, width, height));
    }

    return S_OK;
}

IDCompositionSurface* SurfaceManager::Surface(SurfaceLayerId id) const
{
    return LayerFor(id).surface.get();
}

DXGI_ALPHA_MODE SurfaceManager::AlphaMode(SurfaceLayerId id) const
{
    const SurfaceLayerDefinition* definition = FindLayerDefinition(id);
    return definition != nullptr ? definition->alpha_mode : DXGI_ALPHA_MODE_IGNORE;
}

SurfaceManager::Layer& SurfaceManager::LayerFor(SurfaceLayerId id)
{
    FAIL_FAST_IF(LayerIndex(id) >= layers_.size());
    return layers_[LayerIndex(id)];
}

const SurfaceManager::Layer& SurfaceManager::LayerFor(SurfaceLayerId id) const
{
    FAIL_FAST_IF(LayerIndex(id) >= layers_.size());
    return layers_[LayerIndex(id)];
}

HRESULT SurfaceManager::CreateLayerSurface(SurfaceLayerId id, UINT width, UINT height)
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
