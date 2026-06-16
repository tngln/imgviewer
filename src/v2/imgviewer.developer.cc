#include "imgviewer.developer.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <d2d1helper.h>
#include <quickjs.h>
#include <wil/result_macros.h>

#include "experimental/util.signal.hpp"
#include "imgviewer.hpp"
#include "imgviewer.action.hpp"
#include "imgviewer.messages.hpp"
#include "imgviewer.strings.hpp"
#include "imgviewer.ui.action.hpp"
#include "ui.window.hpp"
#include "v2/imgviewer.script_engine.hpp"
#include "v2/imgviewer.script_ui.hpp"
#include "v2/imgviewer.script_window_root.hpp"
#include "win32.util.hpp"

namespace {

constexpr int kDeveloperInitialWidth = 920;
constexpr int kDeveloperInitialHeight = 720;
constexpr int kDeveloperMinClientWidth = 360;
constexpr int kDeveloperMinClientHeight = 260;
constexpr char kDeveloperScriptRelativePath[] = "scripts/developer_ui.js";
constexpr char kDeveloperCounterSignalName[] = "developer.counter";

class DeveloperScriptUi;

DeveloperScriptUi* ScriptUi(JSContext* context)
{
    return static_cast<DeveloperScriptUi*>(JS_GetContextOpaque(context));
}

class DeveloperScriptUi final : public imgviewer::v2::ScriptWindowRootBase {
public:
    explicit DeveloperScriptUi(imgviewer::v2::ScriptEngine& engine)
        : ScriptWindowRootBase(engine, kDeveloperScriptRelativePath, "imgviewerDeveloperUi", L"Developer TypeScript UI failed"),
          counter_signal_(0)
    {
        ReloadScript();
    }

    ~DeveloperScriptUi() override
    {
        ClearSubscriptions();
    }

    const wchar_t* AccessibilityName() const override { return ImgViewerString(ImgViewerStringId::Developer); }

private:
    struct SignalSubscription final {
        size_t native_id = 0;
        int js_id = 0;
        JSValue callback = JS_UNDEFINED;
    };

    static JSValue SignalsGet(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
    {
        DeveloperScriptUi* ui = ScriptUi(context);
        if (ui == nullptr || argc < 1 || imgviewer::v2::Utf8FromValue(context, argv[0]) != kDeveloperCounterSignalName) {
            return JS_UNDEFINED;
        }
        return JS_NewInt32(context, ui->counter_signal_.Get());
    }

    static JSValue SignalsSet(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
    {
        DeveloperScriptUi* ui = ScriptUi(context);
        if (ui == nullptr || argc < 2 || imgviewer::v2::Utf8FromValue(context, argv[0]) != kDeveloperCounterSignalName) {
            return JS_FALSE;
        }
        int32_t value = 0;
        if (JS_ToInt32(context, &value, argv[1]) < 0) {
            return JS_EXCEPTION;
        }
        return JS_NewBool(context, ui->counter_signal_.Set(value));
    }

    static JSValue SignalsSubscribe(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
    {
        DeveloperScriptUi* ui = ScriptUi(context);
        if (ui == nullptr || argc < 2 || imgviewer::v2::Utf8FromValue(context, argv[0]) != kDeveloperCounterSignalName ||
            !JS_IsFunction(context, argv[1])) {
            return JS_NewInt32(context, 0);
        }

        const int js_id = ui->next_subscription_id_++;
        SignalSubscription subscription;
        subscription.js_id = js_id;
        subscription.callback = JS_DupValue(context, argv[1]);
        subscription.native_id = ui->counter_signal_.Subscribe([ui, js_id](int value) {
            ui->NotifySignalSubscription(js_id, value);
        });
        ui->subscriptions_.push_back(subscription);
        return JS_NewInt32(context, js_id);
    }

    static JSValue SignalsUnsubscribe(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
    {
        DeveloperScriptUi* ui = ScriptUi(context);
        if (ui == nullptr || argc < 1) {
            return JS_FALSE;
        }
        int32_t js_id = 0;
        if (JS_ToInt32(context, &js_id, argv[0]) < 0) {
            return JS_EXCEPTION;
        }
        return JS_NewBool(context, ui->Unsubscribe(js_id));
    }

    void BeforeReload() override
    {
        ClearSubscriptions();
    }

    void InstallCustomGlobals(JSValue global) override
    {
        JSContext* context = Context();
        JSValue signals = JS_NewObject(context);
        SetFunction(signals, "get", SignalsGet, 1);
        SetFunction(signals, "set", SignalsSet, 2);
        SetFunction(signals, "subscribe", SignalsSubscribe, 2);
        SetFunction(signals, "unsubscribe", SignalsUnsubscribe, 1);
        JS_SetPropertyStr(context, global, "signals", signals);
    }

    UiAction CloseAction() const override
    {
        return ImgViewerAction::CloseDeveloper;
    }

    void NotifySignalSubscription(int js_id, int value)
    {
        if (script_context_ == nullptr || Context() == nullptr) {
            return;
        }
        auto it = std::find_if(subscriptions_.begin(), subscriptions_.end(), [js_id](const SignalSubscription& item) {
            return item.js_id == js_id;
        });
        if (it == subscriptions_.end()) {
            return;
        }

        JSContext* context = Context();
        JSValue arg = JS_NewInt32(context, value);
        JSValue result = JS_Call(context, it->callback, JS_UNDEFINED, 1, &arg);
        JS_FreeValue(context, arg);
        if (JS_IsException(result)) {
            JS_FreeValue(context, result);
            script_context_->CaptureException();
            SetError(engine_.TakeExceptionTextUtf8());
            return;
        }
        JS_FreeValue(context, result);
        engine_.PumpJobs();
        invalidate_requested_ = true;
    }

    bool Unsubscribe(int js_id)
    {
        auto it = std::find_if(subscriptions_.begin(), subscriptions_.end(), [js_id](const SignalSubscription& item) {
            return item.js_id == js_id;
        });
        if (it == subscriptions_.end()) {
            return false;
        }
        counter_signal_.Unsubscribe(it->native_id);
        JS_FreeValue(Context(), it->callback);
        subscriptions_.erase(it);
        return true;
    }

    void ClearSubscriptions()
    {
        if (script_context_ == nullptr || Context() == nullptr) {
            subscriptions_.clear();
            return;
        }
        for (const SignalSubscription& subscription : subscriptions_) {
            counter_signal_.Unsubscribe(subscription.native_id);
            JS_FreeValue(Context(), subscription.callback);
        }
        subscriptions_.clear();
    }

    util::Signal<int> counter_signal_;
    std::vector<SignalSubscription> subscriptions_;
    int next_subscription_id_ = 1;
};

struct DeveloperWindowContext final : public UiWindowDelegate {
    HWND owner = nullptr;
    UiWindowHost host;
    bool standalone = false;

    void OnDestroy(UiWindowHost&) override
    {
        if (owner != nullptr) {
            PostMessageW(owner, kImgViewerOwnedWindowDestroyedMessage, 0, reinterpret_cast<LPARAM>(static_cast<UiWindowDelegate*>(this)));
        } else if (standalone) {
            PostQuitMessage(0);
        }
    }

    bool OnUiAction(UiWindowHost& window_host, UiAction action) override
    {
        if (ImgViewerActionFromUiAction(action) == ImgViewerAction::CloseDeveloper) {
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
            util::ApplyMinTrackSize(window_host.Hwnd(), lparam, kDeveloperMinClientWidth, kDeveloperMinClientHeight);
            return win32::WindowMessageResult::Handled();
        }
        return win32::WindowMessageResult::Unhandled();
    }
};

} // namespace

HRESULT OpenImgViewerDeveloperWindow(HWND owner, ImgViewerContext* context)
{
#if defined(IMGVIEWER_ENABLE_DEVELOPER_WINDOW)
    RETURN_HR_IF_NULL(E_INVALIDARG, context);

    if (context->developer_context != nullptr) {
        auto* developer_context = static_cast<DeveloperWindowContext*>(context->developer_context);
        if (developer_context->host.Hwnd() != nullptr && IsWindow(developer_context->host.Hwnd())) {
            ShowWindow(developer_context->host.Hwnd(), SW_SHOWNORMAL);
            SetForegroundWindow(developer_context->host.Hwnd());
            return S_OK;
        }
    }

    HINSTANCE instance = owner != nullptr
        ? reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner, GWLP_HINSTANCE))
        : GetModuleHandleW(nullptr);
    RETURN_HR_IF_NULL(E_UNEXPECTED, instance);

    auto* developer_context = new (std::nothrow) DeveloperWindowContext();
    RETURN_IF_NULL_ALLOC(developer_context);
    developer_context->owner = owner;
    context->developer_context = developer_context;

    auto root = std::make_unique<DeveloperScriptUi>(*context->script_engine);
    const HRESULT create_hr = developer_context->host.Create(
        UiWindowOptions{
            .native = win32::NativeWindowOptions{
                .instance = instance,
                .title = ImgViewerString(ImgViewerStringId::Developer),
                .frame = win32::NativeWindowFrame::Dialog,
                .width = kDeveloperInitialWidth,
                .height = kDeveloperInitialHeight,
                .owner = owner,
            },
            .action_message = kImgViewerUiActionMessage,
            .body_font_size = 9.0f,
            .icon_font_size = 11.0f,
            .script_engine = context->script_engine.get(),
        },
        std::move(root),
        developer_context,
        &context->graphics_device);
    if (FAILED(create_hr)) {
        context->developer_context = nullptr;
        delete developer_context;
        RETURN_IF_FAILED(create_hr);
    }

    developer_context->host.Window().Show(SW_SHOWNORMAL);
    context->interaction.SetModal(ImgViewerModalOwner::Developer);
    return S_OK;
#else
    UNREFERENCED_PARAMETER(owner);
    UNREFERENCED_PARAMETER(context);
    return S_FALSE;
#endif
}

HRESULT RunImgViewerDeveloperWindowApplication()
{
#if defined(IMGVIEWER_ENABLE_DEVELOPER_WINDOW)
    RETURN_IF_FAILED(util::InitializeDpiAwareness());
    const HRESULT co_initialize_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    RETURN_IF_FAILED(co_initialize_result);
    auto co_uninitialize = wil::scope_exit([] { CoUninitialize(); });

    ImgViewerContext context;
    RETURN_IF_FAILED(LoadImgViewerConfig(&context.config));
    RETURN_IF_FAILED(context.graphics_device.Initialize());

    RETURN_IF_FAILED(OpenImgViewerDeveloperWindow(nullptr, &context));
    auto* developer_context = static_cast<DeveloperWindowContext*>(context.developer_context);
    if (developer_context != nullptr) {
        developer_context->standalone = true;
    }

    MSG message = {};
    while (true) {
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result == -1) {
            RETURN_LAST_ERROR();
        }
        if (result == 0) {
            break;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return S_OK;
#else
    return E_NOTIMPL;
#endif
}
