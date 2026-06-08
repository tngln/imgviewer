#include "ui.surface.hpp"

#include <algorithm>
#include <utility>

#include <wil/result_macros.h>

HRESULT UiSurfaceManager::Initialize(IDCompositionDevice* device, IDCompositionVisual* root_visual)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, device);
    RETURN_HR_IF_NULL(E_INVALIDARG, root_visual);

    device_ = device;
    root_visual_ = root_visual;
    return S_OK;
}

HRESULT UiSurfaceManager::RegisterSurface(const UiSurfaceDescriptor& descriptor, UiSurfaceId* id)
{
    RETURN_HR_IF_NULL(E_POINTER, id);
    RETURN_HR_IF_NULL(E_UNEXPECTED, device_);
    RETURN_HR_IF_NULL(E_UNEXPECTED, root_visual_);

    Layer layer;
    layer.id = UiSurfaceId{next_id_++};
    layer.name = descriptor.name != nullptr ? descriptor.name : L"";
    layer.format = descriptor.format;
    layer.alpha_mode = descriptor.alpha_mode;
    layer.z_order = descriptor.z_order;
    layer.auto_resize = descriptor.auto_resize;
    layer.visible = descriptor.initially_visible;
    RETURN_IF_FAILED(device_->CreateVisual(layer.visual.put()));

    if (layer.auto_resize) {
        RETURN_IF_FAILED(CreateLayerSurface(layer, width_, height_));
    }

    if (!layer.visible && layer.visual) {
        RETURN_IF_FAILED(layer.visual->SetContent(nullptr));
    }

    *id = layer.id;
    layers_.push_back(std::move(layer));
    RETURN_IF_FAILED(RebuildVisualOrder());
    return S_OK;
}

void UiSurfaceManager::UnregisterSurface(UiSurfaceId id)
{
    auto found = std::find_if(
        layers_.begin(),
        layers_.end(),
        [id](const Layer& layer) {
            return layer.id == id;
        });
    if (found == layers_.end()) {
        return;
    }

    if (root_visual_ && found->visual) {
        root_visual_->RemoveVisual(found->visual.get());
    }
    layers_.erase(found);
    RebuildVisualOrder();
}

HRESULT UiSurfaceManager::EnsureSurface(UiSurfaceId id, UINT width, UINT height)
{
    Layer* layer = FindLayer(id);
    RETURN_HR_IF_NULL(E_INVALIDARG, layer);
    RETURN_IF_FAILED(CreateLayerSurface(*layer, width, height));
    return S_OK;
}

HRESULT UiSurfaceManager::ResizeAutoSurfaces(UINT width, UINT height)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, device_);

    width_ = width;
    height_ = height;
    for (Layer& layer : layers_) {
        if (layer.auto_resize) {
            RETURN_IF_FAILED(CreateLayerSurface(layer, width, height));
        }
    }

    return S_OK;
}

HRESULT UiSurfaceManager::SetVisible(UiSurfaceId id, bool visible)
{
    Layer* layer = FindLayer(id);
    RETURN_HR_IF_NULL(E_INVALIDARG, layer);
    RETURN_HR_IF_NULL(E_UNEXPECTED, layer->visual);

    layer->visible = visible;
    RETURN_IF_FAILED(layer->visual->SetContent(visible ? layer->surface.get() : nullptr));
    return S_OK;
}

HRESULT UiSurfaceManager::SetZOrder(UiSurfaceId id, int z_order)
{
    Layer* layer = FindLayer(id);
    RETURN_HR_IF_NULL(E_INVALIDARG, layer);
    if (layer->z_order == z_order) {
        return S_OK;
    }
    layer->z_order = z_order;
    RETURN_IF_FAILED(RebuildVisualOrder());
    return S_OK;
}

HRESULT UiSurfaceManager::SetFormat(UiSurfaceId id, DXGI_FORMAT format)
{
    Layer* layer = FindLayer(id);
    RETURN_HR_IF_NULL(E_INVALIDARG, layer);
    if (layer->format == format) {
        return S_OK;
    }

    layer->format = format;
    RETURN_IF_FAILED(CreateLayerSurface(*layer, layer->allocated_width, layer->allocated_height));
    return S_OK;
}

IDCompositionSurface* UiSurfaceManager::Surface(UiSurfaceId id) const
{
    const Layer* layer = FindLayer(id);
    return layer != nullptr ? layer->surface.get() : nullptr;
}

DXGI_FORMAT UiSurfaceManager::Format(UiSurfaceId id) const
{
    const Layer* layer = FindLayer(id);
    return layer != nullptr ? layer->format : DXGI_FORMAT_B8G8R8A8_UNORM;
}

DXGI_ALPHA_MODE UiSurfaceManager::AlphaMode(UiSurfaceId id) const
{
    const Layer* layer = FindLayer(id);
    return layer != nullptr ? layer->alpha_mode : DXGI_ALPHA_MODE_IGNORE;
}

UiSurfaceSize UiSurfaceManager::Size(UiSurfaceId id) const
{
    const Layer* layer = FindLayer(id);
    return layer != nullptr
        ? UiSurfaceSize{layer->allocated_width, layer->allocated_height}
        : UiSurfaceSize{};
}

UiSurfaceManager::Layer* UiSurfaceManager::FindLayer(UiSurfaceId id)
{
    return const_cast<Layer*>(static_cast<const UiSurfaceManager*>(this)->FindLayer(id));
}

const UiSurfaceManager::Layer* UiSurfaceManager::FindLayer(UiSurfaceId id) const
{
    const auto found = std::find_if(
        layers_.begin(),
        layers_.end(),
        [id](const Layer& layer) {
            return layer.id == id;
        });
    return found != layers_.end() ? &*found : nullptr;
}

HRESULT UiSurfaceManager::RebuildVisualOrder()
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, root_visual_);

    std::vector<Layer*> ordered;
    ordered.reserve(layers_.size());
    for (Layer& layer : layers_) {
        if (layer.visual) {
            root_visual_->RemoveVisual(layer.visual.get());
            ordered.push_back(&layer);
        }
    }

    std::sort(
        ordered.begin(),
        ordered.end(),
        [](const Layer* left, const Layer* right) {
            if (left->z_order != right->z_order) {
                return left->z_order < right->z_order;
            }
            return left->id.value < right->id.value;
        });

    IDCompositionVisual* previous_visual = nullptr;
    for (Layer* layer : ordered) {
        RETURN_IF_FAILED(root_visual_->AddVisual(layer->visual.get(), previous_visual != nullptr, previous_visual));
        previous_visual = layer->visual.get();
    }
    return S_OK;
}

HRESULT UiSurfaceManager::CreateLayerSurface(Layer& layer, UINT width, UINT height)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, device_);
    RETURN_HR_IF_NULL(E_UNEXPECTED, layer.visual);

    width = (std::max)(1U, width);
    height = (std::max)(1U, height);
    if (layer.surface &&
        layer.allocated_width == width &&
        layer.allocated_height == height &&
        layer.allocated_format == layer.format) {
        if (layer.visible) {
            RETURN_IF_FAILED(layer.visual->SetContent(layer.surface.get()));
        }
        return S_OK;
    }

    layer.surface.reset();
    RETURN_IF_FAILED(device_->CreateSurface(
        width,
        height,
        layer.format,
        layer.alpha_mode,
        layer.surface.put()));

    RETURN_IF_FAILED(layer.visual->SetContent(layer.visible ? layer.surface.get() : nullptr));
    RETURN_IF_FAILED(layer.visual->SetOffsetX(0.0f));
    RETURN_IF_FAILED(layer.visual->SetOffsetY(0.0f));
    layer.allocated_width = width;
    layer.allocated_height = height;
    layer.allocated_format = layer.format;

    return S_OK;
}
