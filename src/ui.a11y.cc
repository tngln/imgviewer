#include "ui.a11y.hpp"

#include <oleauto.h>
#pragma warning(push)
#pragma warning(disable : 4471)
#include <UIAutomationClient.h>
#pragma warning(pop)

#include <new>
#include <vector>

#include <wil/com.h>
#include <wil/resource.h>
#include <wil/result_macros.h>

#include "com.rc.hpp"
#include "renderer.hpp"

namespace {

constexpr wchar_t kRootName[] = L"ImgViewer";
constexpr int kRootRuntimeId = 1;

SAFEARRAY* MakeRuntimeId(int local_id)
{
    SAFEARRAY* runtime_id = SafeArrayCreateVector(VT_I4, 0, 2);
    if (runtime_id == nullptr) {
        return nullptr;
    }

    LONG index = 0;
    int value = UiaAppendRuntimeId;
    if (FAILED(SafeArrayPutElement(runtime_id, &index, &value))) {
        SafeArrayDestroy(runtime_id);
        return nullptr;
    }

    index = 1;
    value = local_id;
    if (FAILED(SafeArrayPutElement(runtime_id, &index, &value))) {
        SafeArrayDestroy(runtime_id);
        return nullptr;
    }

    return runtime_id;
}

HRESULT SetBstrVariant(const wchar_t* value, VARIANT* variant)
{
    RETURN_HR_IF_NULL(E_POINTER, variant);

    VariantInit(variant);
    variant->vt = VT_BSTR;
    variant->bstrVal = SysAllocString(value);
    RETURN_IF_NULL_ALLOC(variant->bstrVal);

    return S_OK;
}

HRESULT SetIntVariant(int value, VARIANT* variant)
{
    RETURN_HR_IF_NULL(E_POINTER, variant);

    VariantInit(variant);
    variant->vt = VT_I4;
    variant->lVal = value;

    return S_OK;
}

HRESULT SetBoolVariant(bool value, VARIANT* variant)
{
    RETURN_HR_IF_NULL(E_POINTER, variant);

    VariantInit(variant);
    variant->vt = VT_BOOL;
    variant->boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;

    return S_OK;
}

class UiButtonProvider;

class UiRootProvider final :
    public IRawElementProviderSimple,
    public IRawElementProviderFragment,
    public IRawElementProviderFragmentRoot {
public:
    UiRootProvider(HWND hwnd, Renderer* renderer) : hwnd_(hwnd), renderer_(renderer) {}

    HRESULT Initialize();

    IFACEMETHODIMP QueryInterface(REFIID iid, void** object) noexcept override
    {
        if (object == nullptr) {
            return E_POINTER;
        }

        *object = nullptr;
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IRawElementProviderSimple)) {
            *object = static_cast<IRawElementProviderSimple*>(this);
        } else if (iid == __uuidof(IRawElementProviderFragment)) {
            *object = static_cast<IRawElementProviderFragment*>(this);
        } else if (iid == __uuidof(IRawElementProviderFragmentRoot)) {
            *object = static_cast<IRawElementProviderFragmentRoot*>(this);
        } else {
            return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
    }

    IFACEMETHODIMP_(ULONG) AddRef() noexcept override { return rc_.AddRef(); }
    IFACEMETHODIMP_(ULONG) Release() noexcept override { return rc_.Release(this); }

    IFACEMETHODIMP get_ProviderOptions(ProviderOptions* options) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, options);
        *options = ProviderOptions_ServerSideProvider;
        return S_OK;
    }

    IFACEMETHODIMP GetPatternProvider(PATTERNID, IUnknown** provider) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, provider);
        *provider = nullptr;
        return S_OK;
    }

    IFACEMETHODIMP GetPropertyValue(PROPERTYID property_id, VARIANT* value) noexcept override
    {
        if (property_id == UIA_NamePropertyId) {
            return SetBstrVariant(kRootName, value);
        }

        if (property_id == UIA_ControlTypePropertyId) {
            return SetIntVariant(UIA_PaneControlTypeId, value);
        }

        if (property_id == UIA_IsControlElementPropertyId || property_id == UIA_IsContentElementPropertyId) {
            return SetBoolVariant(true, value);
        }

        RETURN_HR_IF_NULL(E_POINTER, value);
        VariantInit(value);
        return S_OK;
    }

    IFACEMETHODIMP get_HostRawElementProvider(IRawElementProviderSimple** provider) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, provider);
        return UiaHostProviderFromHwnd(hwnd_, provider);
    }

    IFACEMETHODIMP Navigate(NavigateDirection direction, IRawElementProviderFragment** provider) noexcept override;

    IFACEMETHODIMP GetRuntimeId(SAFEARRAY** runtime_id) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, runtime_id);
        *runtime_id = MakeRuntimeId(kRootRuntimeId);
        RETURN_IF_NULL_ALLOC(*runtime_id);
        return S_OK;
    }

    IFACEMETHODIMP get_BoundingRectangle(UiaRect* rect) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, rect);

        RECT window_rect = {};
        RETURN_IF_WIN32_BOOL_FALSE(GetWindowRect(hwnd_, &window_rect));
        rect->left = static_cast<double>(window_rect.left);
        rect->top = static_cast<double>(window_rect.top);
        rect->width = static_cast<double>(window_rect.right - window_rect.left);
        rect->height = static_cast<double>(window_rect.bottom - window_rect.top);

        return S_OK;
    }

    IFACEMETHODIMP GetEmbeddedFragmentRoots(SAFEARRAY** roots) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, roots);
        *roots = nullptr;
        return S_OK;
    }

    IFACEMETHODIMP SetFocus() noexcept override
    {
        ::SetFocus(hwnd_);
        return S_OK;
    }

    IFACEMETHODIMP get_FragmentRoot(IRawElementProviderFragmentRoot** root) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, root);
        *root = static_cast<IRawElementProviderFragmentRoot*>(this);
        AddRef();
        return S_OK;
    }

    IFACEMETHODIMP ElementProviderFromPoint(double x, double y, IRawElementProviderFragment** provider) noexcept override;

    IFACEMETHODIMP GetFocus(IRawElementProviderFragment** provider) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, provider);
        *provider = nullptr;
        return S_OK;
    }

    HWND hwnd() const { return hwnd_; }
    Renderer* renderer() const { return renderer_; }
    UiButtonProvider* ProviderAt(size_t index) const;
    UiButtonProvider* ProviderFor(UiElementId id) const;
    UiButtonProvider* NextProvider(UiElementId id) const;
    UiButtonProvider* PreviousProvider(UiElementId id) const;

private:
    HWND hwnd_ = nullptr;
    Renderer* renderer_ = nullptr;
    ComRc<UiRootProvider> rc_;
    std::vector<wil::com_ptr<UiButtonProvider>> button_providers_;
};

class UiButtonProvider final :
    public IRawElementProviderSimple,
    public IRawElementProviderFragment,
    public IInvokeProvider {
public:
    UiButtonProvider(UiRootProvider* root, UiElementId id) : root_(root), id_(id) {}

    IFACEMETHODIMP QueryInterface(REFIID iid, void** object) noexcept override
    {
        if (object == nullptr) {
            return E_POINTER;
        }

        *object = nullptr;
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IRawElementProviderSimple)) {
            *object = static_cast<IRawElementProviderSimple*>(this);
        } else if (iid == __uuidof(IRawElementProviderFragment)) {
            *object = static_cast<IRawElementProviderFragment*>(this);
        } else if (iid == __uuidof(IInvokeProvider)) {
            *object = static_cast<IInvokeProvider*>(this);
        } else {
            return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
    }

    IFACEMETHODIMP_(ULONG) AddRef() noexcept override { return rc_.AddRef(); }
    IFACEMETHODIMP_(ULONG) Release() noexcept override { return rc_.Release(this); }

    IFACEMETHODIMP get_ProviderOptions(ProviderOptions* options) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, options);
        *options = ProviderOptions_ServerSideProvider;
        return S_OK;
    }

    IFACEMETHODIMP GetPatternProvider(PATTERNID pattern_id, IUnknown** provider) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, provider);
        *provider = nullptr;

        if (pattern_id == UIA_InvokePatternId) {
            *provider = static_cast<IInvokeProvider*>(this);
            AddRef();
        }

        return S_OK;
    }

    IFACEMETHODIMP GetPropertyValue(PROPERTYID property_id, VARIANT* value) noexcept override
    {
        const UiButtonMetadata* metadata = root_->renderer()->UiElementMetadata(id_);
        RETURN_HR_IF_NULL(E_UNEXPECTED, metadata);

        if (property_id == UIA_NamePropertyId) {
            return SetBstrVariant(metadata->name, value);
        }

        if (property_id == UIA_AutomationIdPropertyId) {
            return SetBstrVariant(metadata->automation_id, value);
        }

        if (property_id == UIA_ControlTypePropertyId) {
            return SetIntVariant(UIA_ButtonControlTypeId, value);
        }

        if (property_id == UIA_IsEnabledPropertyId || property_id == UIA_IsControlElementPropertyId ||
            property_id == UIA_IsContentElementPropertyId) {
            return SetBoolVariant(true, value);
        }

        RETURN_HR_IF_NULL(E_POINTER, value);
        VariantInit(value);
        return S_OK;
    }

    IFACEMETHODIMP get_HostRawElementProvider(IRawElementProviderSimple** provider) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, provider);
        *provider = nullptr;
        return S_OK;
    }

    IFACEMETHODIMP Navigate(NavigateDirection direction, IRawElementProviderFragment** provider) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, provider);
        *provider = nullptr;

        if (direction == NavigateDirection_Parent) {
            *provider = static_cast<IRawElementProviderFragment*>(root_);
            root_->AddRef();
        } else if (direction == NavigateDirection_NextSibling && root_->NextProvider(id_) != nullptr) {
            *provider = static_cast<IRawElementProviderFragment*>(root_->NextProvider(id_));
            (*provider)->AddRef();
        } else if (direction == NavigateDirection_PreviousSibling && root_->PreviousProvider(id_) != nullptr) {
            *provider = static_cast<IRawElementProviderFragment*>(root_->PreviousProvider(id_));
            (*provider)->AddRef();
        }

        return S_OK;
    }

    IFACEMETHODIMP GetRuntimeId(SAFEARRAY** runtime_id) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, runtime_id);
        const UiButtonMetadata* metadata = root_->renderer()->UiElementMetadata(id_);
        RETURN_HR_IF_NULL(E_UNEXPECTED, metadata);
        *runtime_id = MakeRuntimeId(metadata->runtime_id);
        RETURN_IF_NULL_ALLOC(*runtime_id);
        return S_OK;
    }

    IFACEMETHODIMP get_BoundingRectangle(UiaRect* rect) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, rect);

        const D2D1_RECT_F button_rect = root_->renderer()->UiElementRect(id_);
        POINT origin = {};
        RETURN_IF_WIN32_BOOL_FALSE(ClientToScreen(root_->hwnd(), &origin));

        rect->left = static_cast<double>(origin.x + button_rect.left);
        rect->top = static_cast<double>(origin.y + button_rect.top);
        rect->width = static_cast<double>(button_rect.right - button_rect.left);
        rect->height = static_cast<double>(button_rect.bottom - button_rect.top);

        return S_OK;
    }

    IFACEMETHODIMP GetEmbeddedFragmentRoots(SAFEARRAY** roots) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, roots);
        *roots = nullptr;
        return S_OK;
    }

    IFACEMETHODIMP SetFocus() noexcept override
    {
        ::SetFocus(root_->hwnd());
        return S_OK;
    }

    IFACEMETHODIMP get_FragmentRoot(IRawElementProviderFragmentRoot** root) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, root);
        *root = static_cast<IRawElementProviderFragmentRoot*>(root_);
        root_->AddRef();
        return S_OK;
    }

    IFACEMETHODIMP Invoke() noexcept override
    {
        const UiButtonMetadata* metadata = root_->renderer()->UiElementMetadata(id_);
        RETURN_HR_IF_NULL(E_UNEXPECTED, metadata);
        if (id_ == UiElementId::Test) {
            root_->renderer()->InvokeTestButtonFromAccessibility();
        } else if (id_ == UiElementId::OpenImage) {
            root_->renderer()->InvokeOpenImageFromAccessibility();
        } else {
            root_->renderer()->InvokeUiCommandFromAccessibility(metadata->command);
        }
        return S_OK;
    }

private:
    UiRootProvider* root_ = nullptr;
    UiElementId id_ = UiElementId::None;
    ComRc<UiButtonProvider> rc_;
};

HRESULT UiRootProvider::Initialize()
{
    button_providers_.reserve(renderer_->UiElementCount());
    for (size_t index = 0; index < renderer_->UiElementCount(); ++index) {
        const UiButtonMetadata* metadata = renderer_->UiElementMetadataAt(index);
        RETURN_HR_IF_NULL(E_UNEXPECTED, metadata);

        auto* provider = new (std::nothrow) UiButtonProvider(this, metadata->id);
        RETURN_IF_NULL_ALLOC(provider);

        wil::com_ptr<UiButtonProvider> provider_holder;
        provider_holder.attach(provider);
        button_providers_.push_back(std::move(provider_holder));
    }
    return S_OK;
}

UiButtonProvider* UiRootProvider::ProviderAt(size_t index) const
{
    return index < button_providers_.size() ? button_providers_[index].get() : nullptr;
}

UiButtonProvider* UiRootProvider::ProviderFor(UiElementId id) const
{
    for (size_t index = 0; index < renderer_->UiElementCount(); ++index) {
        const UiButtonMetadata* metadata = renderer_->UiElementMetadataAt(index);
        if (metadata != nullptr && metadata->id == id) {
            return ProviderAt(index);
        }
    }

    return nullptr;
}

UiButtonProvider* UiRootProvider::NextProvider(UiElementId id) const
{
    for (size_t index = 0; index < renderer_->UiElementCount(); ++index) {
        const UiButtonMetadata* metadata = renderer_->UiElementMetadataAt(index);
        if (metadata != nullptr && metadata->id == id) {
            return ProviderAt(index + 1);
        }
    }

    return nullptr;
}

UiButtonProvider* UiRootProvider::PreviousProvider(UiElementId id) const
{
    for (size_t index = 0; index < renderer_->UiElementCount(); ++index) {
        const UiButtonMetadata* metadata = renderer_->UiElementMetadataAt(index);
        if (metadata != nullptr && metadata->id == id) {
            return index > 0 ? ProviderAt(index - 1) : nullptr;
        }
    }

    return nullptr;
}

IFACEMETHODIMP UiRootProvider::Navigate(NavigateDirection direction, IRawElementProviderFragment** provider) noexcept
{
    RETURN_HR_IF_NULL(E_POINTER, provider);
    *provider = nullptr;

    if (direction == NavigateDirection_FirstChild && !button_providers_.empty()) {
        *provider = static_cast<IRawElementProviderFragment*>(button_providers_.front().get());
        (*provider)->AddRef();
    } else if (direction == NavigateDirection_LastChild && !button_providers_.empty()) {
        *provider = static_cast<IRawElementProviderFragment*>(button_providers_.back().get());
        (*provider)->AddRef();
    }

    return S_OK;
}

IFACEMETHODIMP UiRootProvider::ElementProviderFromPoint(double x, double y, IRawElementProviderFragment** provider) noexcept
{
    RETURN_HR_IF_NULL(E_POINTER, provider);
    *provider = nullptr;

    UiaRect button_rect = {};
    for (const auto& button_provider_holder : button_providers_) {
        UiButtonProvider* button_provider = button_provider_holder.get();
        RETURN_IF_FAILED(button_provider->get_BoundingRectangle(&button_rect));
        if (x >= button_rect.left && x < button_rect.left + button_rect.width && y >= button_rect.top &&
            y < button_rect.top + button_rect.height) {
            *provider = static_cast<IRawElementProviderFragment*>(button_provider);
            (*provider)->AddRef();
            return S_OK;
        }
    }

    return S_OK;
}

} // namespace

HRESULT CreateUiAccessibilityProvider(
    HWND hwnd,
    Renderer* renderer,
    IRawElementProviderSimple** provider)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, hwnd);
    RETURN_HR_IF_NULL(E_INVALIDARG, renderer);
    RETURN_HR_IF_NULL(E_POINTER, provider);

    *provider = nullptr;
    auto* root_provider = new (std::nothrow) UiRootProvider(hwnd, renderer);
    RETURN_IF_NULL_ALLOC(root_provider);

    wil::com_ptr<UiRootProvider> root_provider_holder;
    root_provider_holder.attach(root_provider);
    RETURN_IF_FAILED(root_provider->Initialize());

    RETURN_IF_FAILED(root_provider->QueryInterface(IID_PPV_ARGS(provider)));
    return S_OK;
}
