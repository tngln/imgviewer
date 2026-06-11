#include "ui.host_accessibility.hpp"

#include <UIAutomationCoreApi.h>
#include <wil/result_macros.h>

HRESULT ResetUiAccessibilityProvider(
    HWND hwnd,
    UINT action_message,
    UiAccessibilitySource* ui,
    wil::com_ptr<IRawElementProviderSimple>* provider)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, hwnd);
    RETURN_HR_IF(E_INVALIDARG, action_message == 0);
    RETURN_HR_IF_NULL(E_INVALIDARG, ui);
    RETURN_HR_IF_NULL(E_INVALIDARG, provider);

    provider->reset();
    return CreateUiAccessibilityProvider(
        hwnd,
        action_message,
        ui,
        provider->put());
}

win32::WindowMessageResult HandleUiAccessibilityGetObjectMessage(
    HWND hwnd,
    WPARAM wparam,
    LPARAM lparam,
    IRawElementProviderSimple* provider)
{
    if (lparam != UiaRootObjectId || provider == nullptr) {
        return win32::WindowMessageResult::Unhandled();
    }

    return win32::WindowMessageResult::Handled(
        UiaReturnRawElementProvider(
            hwnd,
            wparam,
            lparam,
            provider));
}
