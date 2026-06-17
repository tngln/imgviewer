#include "imgviewer.host.hpp"

#include "imgviewer.hpp"
#include "imgviewer.action.hpp"
#include "imgviewer.config.hpp"
#include "imgviewer.developer.hpp"
#include "imgviewer.edit_geometry.hpp"
#include "imgviewer.host.internal.hpp"
#include "imgviewer.keybindings.hpp"
#include "imgviewer.strings.hpp"
#include "imgviewer.ui.action.hpp"
#include "imgviewer.viewer.hpp"
#include "ui.host_effects.hpp"
#include "ui.host_ime.hpp"
#include "ui.host_popup.hpp"
#include "win32.window.hpp"
#include "win32.util.hpp"

#include <windows.h>
#include <windowsx.h>

#include <commctrl.h>
#include <cwchar>
#include <shellapi.h>
#include <utility>

#include <wil/com.h>
#include <wil/resource.h>
#include <wil/result_macros.h>

bool IsDeveloperCommandLineArgument(const wchar_t* arg)
{
    return arg != nullptr &&
        (_wcsicmp(arg, L"/developer") == 0 ||
            _wcsicmp(arg, L"-developer") == 0 ||
            _wcsicmp(arg, L"--developer") == 0);
}

bool CommandLineRequestsDeveloperWindow(wchar_t** argv, int argc)
{
    for (int index = 1; index < argc; ++index) {
        if (IsDeveloperCommandLineArgument(argv[index])) {
            return true;
        }
    }
    return false;
}

D2D1_POINT_2F GetPointerPoint(HWND hwnd, LPARAM lparam, bool screen_to_client)
{
    POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    if (screen_to_client) {
        ScreenToClient(hwnd, &point);
    }
    return D2D1::Point2F(static_cast<float>(point.x), static_cast<float>(point.y));
}

D2D1_POINT_2F GetScreenPointerPoint(HWND hwnd, LPARAM lparam)
{
    return GetPointerPoint(hwnd, lparam, true);
}

ImgViewerContext* GetImgViewerContext(HWND hwnd)
{
    return static_cast<ImgViewerContext*>(win32::NativeWindow::UserData(hwnd));
}

bool IsCursorInsideWindow(HWND hwnd)
{
    POINT cursor = {};
    RECT window_rect = {};
    return GetCursorPos(&cursor) &&
        GetWindowRect(hwnd, &window_rect) &&
        cursor.x >= window_rect.left &&
        cursor.x < window_rect.right &&
        cursor.y >= window_rect.top &&
        cursor.y < window_rect.bottom;
}

void TrackNonClientMouseLeave(HWND hwnd)
{
    TRACKMOUSEEVENT track_event = {};
    track_event.cbSize = sizeof(track_event);
    track_event.dwFlags = TME_LEAVE | TME_NONCLIENT;
    track_event.hwndTrack = hwnd;
    TrackMouseEvent(&track_event);
}

ImgViewerAction ActionForKeyboardMessage(const ImgViewerContext* context, WPARAM wparam)
{
    if (context == nullptr) {
        return ImgViewerAction::None;
    }

    return ActionForKey(
        context->config.action_bindings,
        static_cast<UINT>(wparam),
        util::IsKeyDown(VK_CONTROL),
        util::IsKeyDown(VK_SHIFT),
        util::IsKeyDown(VK_MENU));
}

size_t KeyActionIndex(WPARAM wparam)
{
    return static_cast<size_t>(static_cast<UINT>(wparam) & 0xFF);
}

ImgViewerHostEffects DispatchUiAction(HWND hwnd, ImgViewerContext* context, UiAction action)
{
    ImgViewerHostEffects effects;
    if (context == nullptr || action == kUiActionNone) {
        return effects;
    }

    if (static_cast<ImgViewerAction>(action.value) == ImgViewerAction::OpenImage) {
        HandleImgViewerOpenImageCommand(hwnd, context);
        return effects;
    }

    ExecuteImgViewerAction(hwnd, context, action);
    effects.sync_popup_modal = true;
    effects.sync_ime = true;
    return effects;
}

void ImgViewerHostEffects::Merge(UiEventResult result, bool request_popup_modal_sync)
{
    handled = handled || result.handled;
    if (result.capture != UiCaptureRequest::None) {
        capture = result.capture;
    }
    if (result.action != kUiActionNone) {
        action = result.action;
    }
    if (result.popup.has_value()) {
        popup = std::move(result.popup);
    }
    sync_popup_modal = sync_popup_modal || request_popup_modal_sync;
    sync_ime = true;
}

void ImgViewerHostEffects::Merge(ImgViewerEventResult result)
{
    handled = handled || result.handled;
    released_capture = released_capture || result.released_capture;
    sync_ime = true;
}

void ApplyHostEffects(HWND hwnd, ImgViewerContext* context, ImgViewerHostEffects effects)
{
    if (context == nullptr) {
        return;
    }

    if (effects.capture == UiCaptureRequest::Capture) {
        context->interaction.BeginPointerCapture(ImgViewerPointerCaptureOwner::Ui);
        ApplyUiCaptureRequest(hwnd, effects.capture);
    } else if (effects.capture == UiCaptureRequest::Release) {
        context->interaction.EndPointerCapture(ImgViewerPointerCaptureOwner::Ui);
        ApplyUiCaptureRequest(hwnd, effects.capture);
    }

    if (effects.popup.has_value()) {
        const UiPopupRequest& popup = effects.popup.value();
        if (SUCCEEDED(context->popup.OpenPopup(popup.origin, MakeJsonPopupContent(popup.state_json)))) {
            effects.sync_popup_modal = true;
        }
    }

    if (effects.begin_pointer_capture != ImgViewerPointerCaptureOwner::None) {
        context->interaction.BeginPointerCapture(effects.begin_pointer_capture);
        ApplyUiCaptureRequest(hwnd, UiCaptureRequest::Capture);
    }

    if (effects.end_pointer_capture != ImgViewerPointerCaptureOwner::None) {
        context->interaction.EndPointerCapture(effects.end_pointer_capture);
        ApplyUiCaptureRequest(hwnd, UiCaptureRequest::Release);
    }

    if (effects.released_capture) {
        context->interaction.ClearPointerCapture();
        ApplyUiCaptureRequest(hwnd, UiCaptureRequest::Release);
    }

    // Full-repaint doctrine: dispatched events repaint the layer (refactor.md 3.4).
    RequestWindowRender(hwnd);

    const ImgViewerHostEffects action_effects = DispatchUiAction(hwnd, context, effects.action);
    if (effects.sync_popup_modal) {
        SyncPopupModal(context);
    }
    if (effects.sync_ime) {
        SyncImgViewerMainWindowIme(hwnd, context);
    }
    if (action_effects.HasFollowUpWork()) {
        ApplyHostEffects(hwnd, context, action_effects);
    }
}

void ApplyMerged(HWND hwnd, ImgViewerContext* context, UiEventResult result)
{
    ImgViewerHostEffects effects;
    effects.Merge(result);
    ApplyHostEffects(hwnd, context, effects);
}

void ApplyMerged(HWND hwnd, ImgViewerContext* context, ImgViewerEventResult result)
{
    ImgViewerHostEffects effects;
    effects.Merge(result);
    ApplyHostEffects(hwnd, context, effects);
}

void ApplyRender(HWND hwnd, ImgViewerContext* context)
{
    ImgViewerHostEffects effects;
    ApplyHostEffects(hwnd, context, effects);
}

void ApplyRenderAndIme(HWND hwnd, ImgViewerContext* context)
{
    ImgViewerHostEffects effects;
    effects.sync_ime = true;
    ApplyHostEffects(hwnd, context, effects);
}

void ApplyImeSync(HWND hwnd, ImgViewerContext* context)
{
    ImgViewerHostEffects effects;
    effects.sync_ime = true;
    ApplyHostEffects(hwnd, context, effects);
}

bool DispatchToPopup(HWND hwnd, ImgViewerContext* context, const UiInputEvent& event)
{
    if (context == nullptr || !context->popup.IsOpen()) {
        return false;
    }

    SyncPopupModal(context);
    UiEventResult result = {};
    DispatchInputEventToPopup(&context->popup, event, &result);
    ImgViewerHostEffects effects;
    effects.Merge(result, true);
    ApplyHostEffects(hwnd, context, effects);
    return result.handled;
}

void ClosePopup(ImgViewerContext* context)
{
    if (context != nullptr) {
        ClosePopupIfOpen(&context->popup);
        context->interaction.ClearModal(ImgViewerModalOwner::Popup);
    }
}

void SyncPopupModal(ImgViewerContext* context)
{
    if (context == nullptr) {
        return;
    }

    if (context->popup.IsOpen()) {
        if (!context->interaction.IsModal(ImgViewerModalOwner::Popup)) {
            context->interaction.SetModal(ImgViewerModalOwner::Popup);
        }
    } else {
        context->interaction.ClearModal(ImgViewerModalOwner::Popup);
    }
}

void SyncKeyboardOwner(ImgViewerContext* context)
{
    if (context == nullptr) {
        return;
    }

    if (context->popup.IsOpen()) {
        context->interaction.SetKeyboardOwner(ImgViewerKeyboardOwner::Popup);
    } else if (context->edit.IsEditingText()) {
        context->interaction.SetKeyboardOwner(ImgViewerKeyboardOwner::EditText);
    } else {
        context->interaction.SetKeyboardOwner(ImgViewerKeyboardOwner::ViewerShortcut);
    }
}

ImgViewerPointerCaptureOwner EditPointerCaptureOwner(const ImgViewerEditController& edit)
{
    if (edit.Tool() == ImgViewerEditTool::Crop) {
        return ImgViewerPointerCaptureOwner::EditCrop;
    }
    if (edit.Tool() == ImgViewerEditTool::PixelSelect) {
        return ImgViewerPointerCaptureOwner::EditPixelSelection;
    }
    return ImgViewerPointerCaptureOwner::EditStroke;
}

void ShowWindowSizeToast(HWND hwnd, ImgViewerContext* context)
{
    if (context == nullptr || IsIconic(hwnd)) {
        return;
    }

    RECT window_rect = {};
    if (!GetWindowRect(hwnd, &window_rect)) {
        return;
    }

    const int width = static_cast<int>(window_rect.right - window_rect.left);
    const int height = static_cast<int>(window_rect.bottom - window_rect.top);
    if (width <= 0 || height <= 0 ||
        (width == context->last_window_size_toast_width && height == context->last_window_size_toast_height)) {
        return;
    }

    context->last_window_size_toast_width = width;
    context->last_window_size_toast_height = height;

    wchar_t toast_text[64] = {};
    swprintf_s(toast_text, L"Window %dx%d", width, height);
    ShowImgViewerToast(hwnd, context, toast_text);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_NCCREATE) {
        const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    using HostMessageHandler = win32::WindowMessageResult (*)(HWND, UINT, WPARAM, LPARAM);
    static constexpr HostMessageHandler kHandlers[] = {
        HandleImgViewerAppMessage,
        HandleImgViewerChromeMessage,
        HandleImgViewerLifecycleMessage,
        HandleImgViewerPointerMessage,
        HandleImgViewerKeyboardMessage,
    };

    for (const HostMessageHandler handler : kHandlers) {
        const win32::WindowMessageResult result = handler(hwnd, message, wparam, lparam);
        if (result.handled) {
            return result.value;
        }
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}
class ImgViewerWindowDelegate final : public win32::NativeWindowDelegate {
public:
    win32::WindowMessageResult OnWindowMessage(
        win32::NativeWindow& window,
        UINT message,
        WPARAM wparam,
        LPARAM lparam) override
    {
        return win32::WindowMessageResult::Handled(WindowProc(window.Hwnd(), message, wparam, lparam));
    }
};

HRESULT RunImgViewerApplicationAsHresult()
{
    int argc = 0;
    wil::unique_hlocal command_line_args{reinterpret_cast<HLOCAL>(CommandLineToArgvW(GetCommandLineW(), &argc))};
    RETURN_LAST_ERROR_IF_NULL(command_line_args.get());
    auto** argv = reinterpret_cast<wchar_t**>(command_line_args.get());
    if (CommandLineRequestsDeveloperWindow(argv, argc)) {
#if defined(IMGVIEWER_ENABLE_DEVELOPER_WINDOW)
        return RunImgViewerDeveloperWindowApplication();
#else
        MessageBoxW(nullptr, L"The Developer window is not enabled in this build.", kImgViewerWindowTitle, MB_OK | MB_ICONINFORMATION);
        return E_NOTIMPL;
#endif
    }

    INITCOMMONCONTROLSEX common_controls = {
        .dwSize = sizeof(common_controls),
        .dwICC = ICC_WIN95_CLASSES,
    };
    InitCommonControlsEx(&common_controls);

    RETURN_IF_FAILED(util::InitializeDpiAwareness());
    const HRESULT co_initialize_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    RETURN_IF_FAILED(co_initialize_result);
    auto co_uninitialize = wil::scope_exit([] { CoUninitialize(); });

    HINSTANCE instance = GetModuleHandleW(nullptr);
    RETURN_LAST_ERROR_IF_NULL(instance);

    ImgViewerContext context;
    RETURN_IF_FAILED(LoadImgViewerConfig(&context.config));
    context.current_window_opacity_percent = context.config.window_opacity_percent;
    context.current_toolbar_scale_percent = context.config.toolbar_scale_percent;
    RETURN_IF_FAILED(ResetImgViewerUi(nullptr, &context));
    const WindowSizeConfig initial_window_size =
        context.config.remember_window_size ? context.config.window_size : WindowSizeConfig{};
    ImgViewerWindowDelegate window_delegate;
    win32::NativeWindow window;
    RETURN_IF_FAILED(window.Create(
        win32::NativeWindowOptions{
            .instance = instance,
            .title = kImgViewerWindowTitle,
            .frame = context.config.borderless_window
                ? win32::NativeWindowFrame::BorderlessMainWindow
                : win32::NativeWindowFrame::MainWindow,
            .width = initial_window_size.width,
            .height = initial_window_size.height,
            .user_data = &context,
        },
        &window_delegate));
    DragAcceptFiles(window.Hwnd(), TRUE);
    context.main_window_ime_context = util::DisableIme(window.Hwnd());
    context.main_window_ime_enabled = false;
    RETURN_IF_FAILED(ApplyImgViewerWindowFrame(window.Hwnd(), &context, false));

    window.Show(SW_SHOWDEFAULT);
    if (argc > 1) {
        LoadImgViewerImageFile(window.Hwnd(), &context, argv[1]);
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
}

int RunImgViewerApplication()
{
    const HRESULT hr = RunImgViewerApplicationAsHresult();
    return SUCCEEDED(hr) ? 0 : 1;
}
