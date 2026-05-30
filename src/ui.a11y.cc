#include "ui.a11y.hpp"

#include <oleauto.h>
#pragma warning(push)
#pragma warning(disable : 4471)
#include <UIAutomationClient.h>
#pragma warning(pop)

#include <algorithm>
#include <new>

#include <wil/com.h>
#include <wil/resource.h>
#include <wil/result_macros.h>

#include "com.rc.hpp"
#include "renderer.hpp"

namespace {

constexpr wchar_t kRootName[] = L"ImgViewer";
constexpr wchar_t kTopMostButtonName[] = L"Top Most";
constexpr wchar_t kTopMostButtonAutomationId[] = L"top-most";
constexpr wchar_t kMinimizeButtonName[] = L"Minimize";
constexpr wchar_t kMinimizeButtonAutomationId[] = L"minimize";
constexpr wchar_t kMaximizeButtonName[] = L"Maximize or Restore";
constexpr wchar_t kMaximizeButtonAutomationId[] = L"maximize-restore";
constexpr wchar_t kCloseButtonName[] = L"Close";
constexpr wchar_t kCloseButtonAutomationId[] = L"close";
constexpr wchar_t kOpenButtonName[] = L"Open Image";
constexpr wchar_t kOpenButtonAutomationId[] = L"open-image";
constexpr wchar_t kTestButtonName[] = L"Test Button";
constexpr wchar_t kTestButtonAutomationId[] = L"test-button";
constexpr int kRootRuntimeId = 1;
constexpr int kTopMostButtonRuntimeId = 2;
constexpr int kMinimizeButtonRuntimeId = 3;
constexpr int kMaximizeButtonRuntimeId = 4;
constexpr int kCloseButtonRuntimeId = 5;
constexpr int kOpenButtonRuntimeId = 6;
constexpr int kTestButtonRuntimeId = 7;

enum class AccessibleButtonId {
    TopMost,
    Minimize,
    MaximizeRestore,
    Close,
    OpenImage,
    Test,
};

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
    UiButtonProvider* top_most_button_provider() const { return top_most_button_provider_.get(); }
    UiButtonProvider* minimize_button_provider() const { return minimize_button_provider_.get(); }
    UiButtonProvider* maximize_button_provider() const { return maximize_button_provider_.get(); }
    UiButtonProvider* close_button_provider() const { return close_button_provider_.get(); }
    UiButtonProvider* open_button_provider() const { return open_button_provider_.get(); }
    UiButtonProvider* test_button_provider() const { return test_button_provider_.get(); }
    UiButtonProvider* ProviderFor(AccessibleButtonId id) const;
    UiButtonProvider* NextProvider(AccessibleButtonId id) const;
    UiButtonProvider* PreviousProvider(AccessibleButtonId id) const;

private:
    HWND hwnd_ = nullptr;
    Renderer* renderer_ = nullptr;
    ComRc<UiRootProvider> rc_;
    wil::com_ptr<UiButtonProvider> top_most_button_provider_;
    wil::com_ptr<UiButtonProvider> minimize_button_provider_;
    wil::com_ptr<UiButtonProvider> maximize_button_provider_;
    wil::com_ptr<UiButtonProvider> close_button_provider_;
    wil::com_ptr<UiButtonProvider> open_button_provider_;
    wil::com_ptr<UiButtonProvider> test_button_provider_;
};

class UiButtonProvider final :
    public IRawElementProviderSimple,
    public IRawElementProviderFragment,
    public IInvokeProvider {
public:
    UiButtonProvider(UiRootProvider* root, AccessibleButtonId id) : root_(root), id_(id) {}

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
        if (property_id == UIA_NamePropertyId) {
            return SetBstrVariant(Name(), value);
        }

        if (property_id == UIA_AutomationIdPropertyId) {
            return SetBstrVariant(AutomationId(), value);
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
        *runtime_id = MakeRuntimeId(RuntimeId());
        RETURN_IF_NULL_ALLOC(*runtime_id);
        return S_OK;
    }

    IFACEMETHODIMP get_BoundingRectangle(UiaRect* rect) noexcept override
    {
        RETURN_HR_IF_NULL(E_POINTER, rect);

        const D2D1_RECT_F button_rect = root_->renderer()->UiElementRect(ElementId());
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
        if (id_ == AccessibleButtonId::Test) {
            root_->renderer()->InvokeTestButtonFromAccessibility();
        } else if (id_ == AccessibleButtonId::OpenImage) {
            root_->renderer()->InvokeOpenImageFromAccessibility();
        } else {
            root_->renderer()->InvokeUiCommandFromAccessibility(Command());
        }
        return S_OK;
    }

private:
    const wchar_t* Name() const
    {
        switch (id_) {
        case AccessibleButtonId::TopMost:
            return kTopMostButtonName;
        case AccessibleButtonId::Minimize:
            return kMinimizeButtonName;
        case AccessibleButtonId::MaximizeRestore:
            return kMaximizeButtonName;
        case AccessibleButtonId::Close:
            return kCloseButtonName;
        case AccessibleButtonId::OpenImage:
            return kOpenButtonName;
        case AccessibleButtonId::Test:
        default:
            return kTestButtonName;
        }
    }

    const wchar_t* AutomationId() const
    {
        switch (id_) {
        case AccessibleButtonId::TopMost:
            return kTopMostButtonAutomationId;
        case AccessibleButtonId::Minimize:
            return kMinimizeButtonAutomationId;
        case AccessibleButtonId::MaximizeRestore:
            return kMaximizeButtonAutomationId;
        case AccessibleButtonId::Close:
            return kCloseButtonAutomationId;
        case AccessibleButtonId::OpenImage:
            return kOpenButtonAutomationId;
        case AccessibleButtonId::Test:
        default:
            return kTestButtonAutomationId;
        }
    }

    int RuntimeId() const
    {
        switch (id_) {
        case AccessibleButtonId::TopMost:
            return kTopMostButtonRuntimeId;
        case AccessibleButtonId::Minimize:
            return kMinimizeButtonRuntimeId;
        case AccessibleButtonId::MaximizeRestore:
            return kMaximizeButtonRuntimeId;
        case AccessibleButtonId::Close:
            return kCloseButtonRuntimeId;
        case AccessibleButtonId::OpenImage:
            return kOpenButtonRuntimeId;
        case AccessibleButtonId::Test:
        default:
            return kTestButtonRuntimeId;
        }
    }

    UiElementId ElementId() const
    {
        switch (id_) {
        case AccessibleButtonId::TopMost:
            return UiElementId::TopMost;
        case AccessibleButtonId::Minimize:
            return UiElementId::Minimize;
        case AccessibleButtonId::MaximizeRestore:
            return UiElementId::MaximizeRestore;
        case AccessibleButtonId::Close:
            return UiElementId::Close;
        case AccessibleButtonId::OpenImage:
            return UiElementId::OpenImage;
        case AccessibleButtonId::Test:
        default:
            return UiElementId::Test;
        }
    }

    UiCommand Command() const
    {
        switch (id_) {
        case AccessibleButtonId::TopMost:
            return UiCommand::ToggleTopMost;
        case AccessibleButtonId::Minimize:
            return UiCommand::Minimize;
        case AccessibleButtonId::MaximizeRestore:
            return UiCommand::ToggleMaximize;
        case AccessibleButtonId::Close:
            return UiCommand::Close;
        case AccessibleButtonId::OpenImage:
            return UiCommand::OpenImage;
        default:
            return UiCommand::None;
        }
    }

    UiRootProvider* root_ = nullptr;
    AccessibleButtonId id_ = AccessibleButtonId::Test;
    ComRc<UiButtonProvider> rc_;
};

HRESULT UiRootProvider::Initialize()
{
    auto* top_most_provider = new (std::nothrow) UiButtonProvider(this, AccessibleButtonId::TopMost);
    RETURN_IF_NULL_ALLOC(top_most_provider);
    top_most_button_provider_.attach(top_most_provider);

    auto* minimize_provider = new (std::nothrow) UiButtonProvider(this, AccessibleButtonId::Minimize);
    RETURN_IF_NULL_ALLOC(minimize_provider);
    minimize_button_provider_.attach(minimize_provider);

    auto* maximize_provider = new (std::nothrow) UiButtonProvider(this, AccessibleButtonId::MaximizeRestore);
    RETURN_IF_NULL_ALLOC(maximize_provider);
    maximize_button_provider_.attach(maximize_provider);

    auto* close_provider = new (std::nothrow) UiButtonProvider(this, AccessibleButtonId::Close);
    RETURN_IF_NULL_ALLOC(close_provider);
    close_button_provider_.attach(close_provider);

    auto* open_provider = new (std::nothrow) UiButtonProvider(this, AccessibleButtonId::OpenImage);
    RETURN_IF_NULL_ALLOC(open_provider);
    open_button_provider_.attach(open_provider);

    auto* test_provider = new (std::nothrow) UiButtonProvider(this, AccessibleButtonId::Test);
    RETURN_IF_NULL_ALLOC(test_provider);
    test_button_provider_.attach(test_provider);
    return S_OK;
}

UiButtonProvider* UiRootProvider::ProviderFor(AccessibleButtonId id) const
{
    switch (id) {
    case AccessibleButtonId::TopMost:
        return top_most_button_provider_.get();
    case AccessibleButtonId::Minimize:
        return minimize_button_provider_.get();
    case AccessibleButtonId::MaximizeRestore:
        return maximize_button_provider_.get();
    case AccessibleButtonId::Close:
        return close_button_provider_.get();
    case AccessibleButtonId::OpenImage:
        return open_button_provider_.get();
    case AccessibleButtonId::Test:
    default:
        return test_button_provider_.get();
    }
}

UiButtonProvider* UiRootProvider::NextProvider(AccessibleButtonId id) const
{
    switch (id) {
    case AccessibleButtonId::TopMost:
        return minimize_button_provider_.get();
    case AccessibleButtonId::Minimize:
        return maximize_button_provider_.get();
    case AccessibleButtonId::MaximizeRestore:
        return close_button_provider_.get();
    case AccessibleButtonId::Close:
        return open_button_provider_.get();
    case AccessibleButtonId::OpenImage:
        return test_button_provider_.get();
    default:
        return nullptr;
    }
}

UiButtonProvider* UiRootProvider::PreviousProvider(AccessibleButtonId id) const
{
    switch (id) {
    case AccessibleButtonId::Minimize:
        return top_most_button_provider_.get();
    case AccessibleButtonId::MaximizeRestore:
        return minimize_button_provider_.get();
    case AccessibleButtonId::Close:
        return maximize_button_provider_.get();
    case AccessibleButtonId::OpenImage:
        return close_button_provider_.get();
    case AccessibleButtonId::Test:
        return open_button_provider_.get();
    default:
        return nullptr;
    }
}

IFACEMETHODIMP UiRootProvider::Navigate(NavigateDirection direction, IRawElementProviderFragment** provider) noexcept
{
    RETURN_HR_IF_NULL(E_POINTER, provider);
    *provider = nullptr;

    if (direction == NavigateDirection_FirstChild) {
        *provider = static_cast<IRawElementProviderFragment*>(top_most_button_provider_.get());
        (*provider)->AddRef();
    } else if (direction == NavigateDirection_LastChild) {
        *provider = static_cast<IRawElementProviderFragment*>(test_button_provider_.get());
        (*provider)->AddRef();
    }

    return S_OK;
}

IFACEMETHODIMP UiRootProvider::ElementProviderFromPoint(double x, double y, IRawElementProviderFragment** provider) noexcept
{
    RETURN_HR_IF_NULL(E_POINTER, provider);
    *provider = nullptr;

    UiaRect button_rect = {};
    constexpr AccessibleButtonId ids[] = {
        AccessibleButtonId::TopMost,
        AccessibleButtonId::Minimize,
        AccessibleButtonId::MaximizeRestore,
        AccessibleButtonId::Close,
        AccessibleButtonId::OpenImage,
        AccessibleButtonId::Test,
    };
    for (const AccessibleButtonId id : ids) {
        UiButtonProvider* button_provider = ProviderFor(id);
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
