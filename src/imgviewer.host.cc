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
#include "imgviewer.ui.hpp"
#include "imgviewer.viewer.hpp"
#include "math.hpp"
#include "ui.tooltip.hpp"
#include "win32.window.hpp"
#include "win32.util.hpp"

#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <commctrl.h>
#include <cwchar>
#include <cmath>
#include <imm.h>
#include <shellapi.h>
#include <string>
#include <vector>

#include <wil/com.h>
#include <wil/resource.h>
#include <wil/result_macros.h>

constexpr wchar_t kWindowClassName[] = L"ImgViewerWindow";
constexpr float kEdgeClickDragCancelDistance = 6.0f;

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

std::wstring ImeCompositionString(HWND hwnd, LPARAM lparam, DWORD string_type)
{
    if ((lparam & string_type) == 0) {
        return {};
    }

    HIMC ime = ImmGetContext(hwnd);
    if (ime == nullptr) {
        return {};
    }

    const LONG bytes = ImmGetCompositionStringW(ime, string_type, nullptr, 0);
    std::wstring text(bytes > 0 ? static_cast<size_t>(bytes) / sizeof(wchar_t) : 0, L'\0');
    if (!text.empty()) {
        ImmGetCompositionStringW(ime, string_type, text.data(), bytes);
    }
    ImmReleaseContext(hwnd, ime);
    return text;
}

bool EditingTextCaretPoint(ImgViewerContext* context, D2D1_POINT_2F* point)
{
    if (context == nullptr || point == nullptr) {
        return false;
    }

    const ImgViewerEditSnapshot edit = context->edit.Snapshot();
    if (!edit.active || !edit.editing_text || edit.editing_text_index >= edit.texts.size()) {
        return false;
    }

    constexpr float kPaddingX = 6.0f;
    constexpr float kPaddingY = 4.0f;
    const ImgViewerEditText& text = edit.texts[edit.editing_text_index];
    const float font_size = (std::max)(6.0f, text.style.font_size);
    wil::com_ptr<IDWriteTextFormat> format;
    if (FAILED(context->graphics_device.DWriteFactory()->CreateTextFormat(
            text.style.font_family.c_str(),
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            font_size,
            L"",
            format.put()))) {
        return false;
    }
    if (FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP))) {
        return false;
    }

    const TextEditState& edit_state = edit.editing_text_state;
    const std::wstring display_text = edit_state.DisplayText().empty() ? L" " : edit_state.DisplayText();
    const float width = (std::max)(48.0f, static_cast<float>(display_text.size()) * font_size * 0.55f + kPaddingX * 2.0f);
    wil::com_ptr<IDWriteTextLayout> layout;
    if (FAILED(context->graphics_device.DWriteFactory()->CreateTextLayout(
            display_text.c_str(),
            static_cast<UINT32>(display_text.size()),
            format.get(),
            width,
            4096.0f,
            layout.put()))) {
        return false;
    }

    const D2D1_POINT_2F origin = D2D1::Point2F(text.origin.x + kPaddingX, text.origin.y + kPaddingY);
    D2D1_POINT_2F caret_top = origin;
    D2D1_POINT_2F caret_bottom = origin;
    if (!edit_state.CaretMetrics(layout.get(), origin, &caret_top, &caret_bottom)) {
        return false;
    }
    *point = caret_bottom;
    return true;
}

D2D1_POINT_2F DocumentPointToViewportPoint(const ImgViewerSnapshot& image, const ImgViewerEditSnapshot& edit, D2D1_POINT_2F point, D2D1_SIZE_U viewport_size)
{
    const D2D1_SIZE_U preview_size = imgviewer_edit_geometry::EditPreviewSize(image.pixel_size, edit.rotation_quadrants);
    const float image_scale = math::FitScale(preview_size, viewport_size) * image.zoom_multiplier;
    const D2D1_POINT_2F viewport_center = D2D1::Point2F(
        static_cast<float>(viewport_size.width) * 0.5f,
        static_cast<float>(viewport_size.height) * 0.5f);
    const D2D1_POINT_2F preview_view_center = imgviewer_edit_geometry::SourcePointToEditPreviewPoint(
        image.view_center,
        image.pixel_size,
        edit.rotation_quadrants);
    const D2D1_MATRIX_3X2_F transform =
        imgviewer_edit_geometry::SourceToEditPreviewTransform(image.pixel_size, edit.rotation_quadrants) *
        D2D1::Matrix3x2F::Translation(-preview_view_center.x, -preview_view_center.y) *
        D2D1::Matrix3x2F::Scale(image_scale, image_scale) *
        D2D1::Matrix3x2F::Scale(
            image.flipped_horizontal ? -1.0f : 1.0f,
            image.flipped_vertical ? -1.0f : 1.0f,
            D2D1::Point2F(0.0f, 0.0f)) *
        D2D1::Matrix3x2F::Rotation(image.rotation_degrees, D2D1::Point2F(0.0f, 0.0f)) *
        D2D1::Matrix3x2F::Translation(viewport_center.x, viewport_center.y);
    return D2D1::Point2F(
        point.x * transform._11 + point.y * transform._21 + transform._31,
        point.x * transform._12 + point.y * transform._22 + transform._32);
}

void PositionMainWindowIme(HWND hwnd, ImgViewerContext* context)
{
    if (context == nullptr || !context->edit.IsEditingText()) {
        return;
    }

    D2D1_POINT_2F caret_document_point = {};
    if (!EditingTextCaretPoint(context, &caret_document_point)) {
        return;
    }

    const ImgViewerSnapshot image = context->viewer.Snapshot();
    const ImgViewerEditSnapshot edit = context->edit.Snapshot();
    const D2D1_SIZE_U viewport_size = context->renderer.ViewportPixelSize();
    if (image.bitmap == nullptr || viewport_size.width == 0 || viewport_size.height == 0) {
        return;
    }

    const D2D1_POINT_2F caret_viewport_point = DocumentPointToViewportPoint(image, edit, caret_document_point, viewport_size);
    HIMC ime = ImmGetContext(hwnd);
    if (ime == nullptr) {
        return;
    }
    COMPOSITIONFORM form = {};
    form.dwStyle = CFS_POINT;
    form.ptCurrentPos = POINT{
        static_cast<LONG>(std::floor(caret_viewport_point.x)),
        static_cast<LONG>(std::floor(caret_viewport_point.y)),
    };
    ImmSetCompositionWindow(ime, &form);
    ImmReleaseContext(hwnd, ime);
}

void SyncPopupModal(ImgViewerContext* context);

ImgViewerHostEffects DispatchUiAction(HWND hwnd, ImgViewerContext* context, UiAction action)
{
    ImgViewerHostEffects effects;
    if (context == nullptr || action == kUiActionNone) {
        return effects;
    }

    if (context->ui.Root() != nullptr && context->ui.Root()->HandleUiAction(action, &context->popup)) {
        effects.needs_render = true;
        effects.sync_popup_modal = true;
        effects.sync_ime = true;
        return effects;
    }

    const ImgViewerAction viewer_action = ImgViewerActionFromUiAction(action);
    if (viewer_action == ImgViewerAction::OpenImage) {
        HandleImgViewerOpenImageCommand(hwnd, context);
        return effects;
    }

    ExecuteImgViewerAction(hwnd, context, viewer_action);
    effects.sync_popup_modal = true;
    effects.sync_ime = true;
    return effects;
}

void ImgViewerHostEffects::Merge(UiEventResult result, bool request_popup_modal_sync)
{
    handled = handled || result.handled;
    needs_render = needs_render || result.needs_render;
    if (result.capture != UiCaptureRequest::None) {
        capture = result.capture;
    }
    if (result.action != kUiActionNone) {
        action = result.action;
    }
    if (result.effect_target != UiElementId::None) {
        effect_target = result.effect_target;
    }
    sync_popup_modal = sync_popup_modal || request_popup_modal_sync;
    sync_ime = true;
}

void ImgViewerHostEffects::Merge(ImgViewerEventResult result)
{
    handled = handled || result.handled;
    needs_render = needs_render || result.needs_render;
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
        SetCapture(hwnd);
    } else if (effects.capture == UiCaptureRequest::Release) {
        context->interaction.EndPointerCapture(ImgViewerPointerCaptureOwner::Ui);
        ReleaseCapture();
    }

    if (effects.begin_pointer_capture != ImgViewerPointerCaptureOwner::None) {
        context->interaction.BeginPointerCapture(effects.begin_pointer_capture);
        SetCapture(hwnd);
    }

    if (effects.end_pointer_capture != ImgViewerPointerCaptureOwner::None) {
        context->interaction.EndPointerCapture(effects.end_pointer_capture);
        ReleaseCapture();
    }

    if (effects.released_capture) {
        context->interaction.ClearPointerCapture();
        ReleaseCapture();
    }

    if (effects.effect_target != UiElementId::None && context->ui.Root() != nullptr) {
        context->ui.Root()->ApplyElementEffect(effects.effect_target);
        effects.needs_render = true;
    }

    if (effects.needs_render) {
        RenderImgViewer(context);
        PositionMainWindowIme(hwnd, context);
    }

    const ImgViewerHostEffects action_effects = DispatchUiAction(hwnd, context, effects.action);
    if (effects.sync_popup_modal) {
        SyncPopupModal(context);
    }
    if (effects.sync_ime) {
        SyncImgViewerMainWindowIme(hwnd, context);
    }
    if (action_effects.needs_render || action_effects.capture != UiCaptureRequest::None || action_effects.released_capture ||
        action_effects.begin_pointer_capture != ImgViewerPointerCaptureOwner::None ||
        action_effects.end_pointer_capture != ImgViewerPointerCaptureOwner::None ||
        action_effects.action != kUiActionNone || action_effects.effect_target != UiElementId::None ||
        action_effects.sync_popup_modal || action_effects.sync_ime) {
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
    effects.needs_render = true;
    ApplyHostEffects(hwnd, context, effects);
}

void ApplyRenderAndIme(HWND hwnd, ImgViewerContext* context)
{
    ImgViewerHostEffects effects;
    effects.needs_render = true;
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
    const UiEventResult result = context->popup.OnInputEvent(event);
    ImgViewerHostEffects effects;
    effects.Merge(result, true);
    ApplyHostEffects(hwnd, context, effects);
    return result.handled;
}

void ClosePopup(ImgViewerContext* context)
{
    if (context != nullptr && context->popup.IsOpen()) {
        context->popup.Close();
        context->interaction.ClearModal(ImgViewerModalOwner::Popup);
    }
}

void SyncPopupModal(ImgViewerContext* context)
{
    if (context == nullptr) {
        return;
    }

    if (context->popup.IsOpen()) {
        if (context->interaction.Modal() != ImgViewerModalOwner::Popup) {
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
    } else if (context->ui.FocusedElement() != UiElementId::None) {
        context->interaction.SetKeyboardOwner(ImgViewerKeyboardOwner::UiFocus);
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

float DistanceSquared(D2D1_POINT_2F left, D2D1_POINT_2F right)
{
    const float dx = left.x - right.x;
    const float dy = left.y - right.y;
    return dx * dx + dy * dy;
}

void ClearPendingEdgeClick(HWND hwnd, ImgViewerContext* context)
{
    if (context == nullptr) {
        return;
    }

    context->pending_edge_click_action = ImgViewerAction::None;
    context->pending_edge_click_point = {};
    context->interaction.EndPointerCapture(ImgViewerPointerCaptureOwner::EdgeClickNavigation);
    if (hwnd != nullptr && GetCapture() == hwnd) {
        ReleaseCapture();
    }
}

ImgViewerAction EdgeClickActionAtPoint(const ImgViewerContext* context, D2D1_POINT_2F point, bool require_no_capture)
{
    if (context == nullptr ||
        !context->config.edge_click_navigation ||
        context->interaction.Modal() != ImgViewerModalOwner::None ||
        (require_no_capture && context->interaction.HasPointerCapture()) ||
        context->interaction.CanvasOwner() != ImgViewerCanvasOwner::Viewer) {
        return ImgViewerAction::None;
    }

    const D2D1_SIZE_U viewport_size = context->renderer.ViewportPixelSize();
    if (viewport_size.width == 0 || viewport_size.height == 0) {
        return ImgViewerAction::None;
    }

    const int zone_percent = ClampEdgeClickNavigationZonePercent(context->config.edge_click_navigation_zone_percent);
    const float zone_width = (std::max)(1.0f, static_cast<float>(viewport_size.width) * static_cast<float>(zone_percent) / 100.0f);
    if (point.x >= 0.0f && point.x < zone_width) {
        return ImgViewerAction::PreviousImage;
    }
    if (point.x >= static_cast<float>(viewport_size.width) - zone_width &&
        point.x < static_cast<float>(viewport_size.width)) {
        return ImgViewerAction::NextImage;
    }

    return ImgViewerAction::None;
}

bool CancelPendingEdgeClickIfDragged(HWND hwnd, ImgViewerContext* context, D2D1_POINT_2F point)
{
    if (context == nullptr ||
        context->interaction.PointerCapture() != ImgViewerPointerCaptureOwner::EdgeClickNavigation) {
        return false;
    }

    if (DistanceSquared(context->pending_edge_click_point, point) >
        kEdgeClickDragCancelDistance * kEdgeClickDragCancelDistance) {
        ClearPendingEdgeClick(hwnd, context);
    }
    return true;
}

bool CommitPendingEdgeClick(HWND hwnd, ImgViewerContext* context, D2D1_POINT_2F point)
{
    if (context == nullptr ||
        context->interaction.PointerCapture() != ImgViewerPointerCaptureOwner::EdgeClickNavigation) {
        return false;
    }

    const ImgViewerAction action = context->pending_edge_click_action;
    const bool still_in_same_zone = action != ImgViewerAction::None && EdgeClickActionAtPoint(context, point, false) == action;
    ClearPendingEdgeClick(hwnd, context);
    if (still_in_same_zone) {
        ExecuteImgViewerAction(hwnd, context, action);
    }
    return true;
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
    SetImgViewerLanguage(context.config.language);
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
            .class_name = kWindowClassName,
            .title = kImgViewerWindowTitle,
            .style = ImgViewerWindowStyle(context.config.borderless_window),
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
    HWND tooltip = context.tooltip.get();
    RETURN_IF_FAILED(InitializeUiTooltips(window.Hwnd(), &tooltip, context.ui));
    if (context.tooltip.get() != tooltip) {
        context.tooltip.reset(tooltip);
    }

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

void SyncImgViewerMainWindowIme(HWND hwnd, ImgViewerContext* context)
{
    if (hwnd == nullptr || context == nullptr) {
        return;
    }

    const bool should_enable = context->edit.IsEditingText() && context->main_window_ime_context != nullptr;
    if (context->main_window_ime_enabled == should_enable) {
        if (should_enable) {
            PositionMainWindowIme(hwnd, context);
        }
        return;
    }

    if (!should_enable) {
        if (HIMC ime = ImmGetContext(hwnd)) {
            ImmNotifyIME(ime, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
            ImmReleaseContext(hwnd, ime);
        }
    }

    util::AssociateImeContext(hwnd, should_enable ? context->main_window_ime_context : nullptr);
    context->main_window_ime_enabled = should_enable;
    if (should_enable) {
        PositionMainWindowIme(hwnd, context);
    }
}

int RunImgViewerApplication()
{
    const HRESULT hr = RunImgViewerApplicationAsHresult();
    return SUCCEEDED(hr) ? 0 : 1;
}
