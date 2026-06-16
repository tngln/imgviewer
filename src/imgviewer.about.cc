#include "imgviewer.about.hpp"

#include <memory>

#include <wil/result_macros.h>

#include "imgviewer.hpp"
#include "imgviewer.action.hpp"
#include "imgviewer.messages.hpp"
#include "imgviewer.strings.hpp"
#include "imgviewer.ui.action.hpp"
#include "ui.window.hpp"
#include "v2/imgviewer.script_engine.hpp"
#include "v2/imgviewer.script_window_root.hpp"
#include "win32.util.hpp"

namespace {

constexpr int kAboutInitialWidth = 560;
constexpr int kAboutInitialHeight = 520;
constexpr int kAboutMinClientWidth = 240;
constexpr int kAboutMinClientHeight = 210;
constexpr char kAboutScriptRelativePath[] = "scripts/about_ui.js";

class AboutScriptUi final : public imgviewer::v2::ScriptWindowRootBase {
public:
    explicit AboutScriptUi(imgviewer::v2::ScriptEngine& engine)
        : ScriptWindowRootBase(engine, kAboutScriptRelativePath, "imgviewerAboutUi", L"About TypeScript UI failed")
    {
        ReloadScript();
    }

    const wchar_t* AccessibilityName() const override { return ImgViewerString(ImgViewerStringId::AboutImgViewer); }

private:
    UiAction CloseAction() const override
    {
        return ImgViewerAction::CloseAbout;
    }
};

struct AboutWindowContext final : public UiWindowDelegate {
    HWND owner = nullptr;
    UiWindowHost host;

    void OnDestroy(UiWindowHost&) override
    {
        if (owner != nullptr) {
            PostMessageW(owner, kImgViewerOwnedWindowDestroyedMessage, 0, reinterpret_cast<LPARAM>(static_cast<UiWindowDelegate*>(this)));
        }
    }

    bool OnUiAction(UiWindowHost& window_host, UiAction action) override
    {
        if (ImgViewerActionFromUiAction(action) == ImgViewerAction::CloseAbout) {
            window_host.Close();
            return true;
        }
        return false;
    }

    win32::WindowMessageResult OnUnhandledMessage(
        UiWindowHost& window_host,
        UINT message,
        WPARAM,
        LPARAM lparam) override
    {
        if (message == WM_GETMINMAXINFO) {
            util::ApplyMinTrackSize(window_host.Hwnd(), lparam, kAboutMinClientWidth, kAboutMinClientHeight);
            return win32::WindowMessageResult::Handled();
        }
        return win32::WindowMessageResult::Unhandled();
    }
};

} // namespace

HRESULT OpenImgViewerAboutWindow(HWND owner, ImgViewerContext* context)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, owner);
    RETURN_HR_IF_NULL(E_INVALIDARG, context);

    if (context->about_context != nullptr) {
        auto* about_context = static_cast<AboutWindowContext*>(context->about_context);
        if (about_context->host.Hwnd() != nullptr && IsWindow(about_context->host.Hwnd())) {
            ShowWindow(about_context->host.Hwnd(), SW_SHOWNORMAL);
            SetForegroundWindow(about_context->host.Hwnd());
            return S_OK;
        }
    }

    HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner, GWLP_HINSTANCE));
    RETURN_HR_IF_NULL(E_UNEXPECTED, instance);

    auto* about_context = new (std::nothrow) AboutWindowContext();
    RETURN_IF_NULL_ALLOC(about_context);
    about_context->owner = owner;
    context->about_context = about_context;

    auto root = std::make_unique<AboutScriptUi>(*context->script_engine);
    const HRESULT create_hr = about_context->host.Create(
        UiWindowOptions{
            .native = win32::NativeWindowOptions{
                .instance = instance,
                .title = ImgViewerString(ImgViewerStringId::AboutImgViewer),
                .frame = win32::NativeWindowFrame::Dialog,
                .width = kAboutInitialWidth,
                .height = kAboutInitialHeight,
                .owner = owner,
            },
            .action_message = kImgViewerUiActionMessage,
            .body_font_size = 9.0f,
            .icon_font_size = 11.0f,
            .script_engine = context->script_engine.get(),
        },
        std::move(root),
        about_context,
        &context->graphics_device);
    if (FAILED(create_hr)) {
        context->about_context = nullptr;
        delete about_context;
        RETURN_IF_FAILED(create_hr);
    }

    about_context->host.Window().Show(SW_SHOWNORMAL);
    context->interaction.SetModal(ImgViewerModalOwner::About);
    return S_OK;
}
