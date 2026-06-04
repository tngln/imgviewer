#include "imgviewer.hpp"

#include <algorithm>
#include <optional>
#include <string>

#include <wil/result_macros.h>
#include <wil/resource.h>

#include "win32.dialog.hpp"
#include "win32.util.hpp"
#include "imgviewer.messages.hpp"
#include "imgviewer.settings.hpp"
#include "imgviewer.ui.action.hpp"
#include "imgviewer.ui.hpp"
#include "ui.tooltip.hpp"

namespace {

constexpr UINT kToastDurationMs = 2000;

bool NavigateImageFile(HWND hwnd, ImgViewerContext* context, int direction);
bool CopyTextToClipboard(HWND hwnd, const wchar_t* text);
void SetColorPickerActive(ImgViewerContext* context, bool active);

} // namespace

ImgViewerContext::ImgViewerContext() : ui(std::make_unique<ImgViewerUi>()) {}

HRESULT RenderImgViewer(ImgViewerContext* context)
{
    if (context == nullptr) {
        return S_OK;
    }

    RETURN_IF_FAILED(context->renderer.Render(context->viewer, context->ui));
    return S_OK;
}

DWORD ImgViewerWindowStyle(bool borderless)
{
    if (borderless) {
        return WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    }

    return WS_OVERLAPPEDWINDOW;
}

HRESULT ApplyImgViewerWindowFrame(HWND hwnd, ImgViewerContext* context, bool hide_for_transition)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, hwnd);
    RETURN_HR_IF_NULL(E_INVALIDARG, context);

    const bool was_visible = IsWindowVisible(hwnd) != FALSE;
    const bool was_zoomed = IsZoomed(hwnd) != FALSE;
    if (hide_for_transition && was_visible) {
        ShowWindow(hwnd, SW_HIDE);
    }

    SetLastError(ERROR_SUCCESS);
    const LONG_PTR previous_style = SetWindowLongPtrW(
        hwnd,
        GWL_STYLE,
        static_cast<LONG_PTR>(ImgViewerWindowStyle(context->config.borderless_window)));
    if (previous_style == 0 && GetLastError() != ERROR_SUCCESS) {
        RETURN_LAST_ERROR();
    }

    RETURN_IF_FAILED(util::ApplyDwmFrame(hwnd, context->config.borderless_window));
    RETURN_IF_WIN32_BOOL_FALSE(SetWindowPos(
        hwnd,
        nullptr,
        0,
        0,
        0,
        0,
        SWP_FRAMECHANGED | SWP_NOREDRAW | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));
    RETURN_IF_FAILED(context->renderer.Resize());
    SyncWindowState(hwnd, &context->ui);
    UpdateUiTooltipRects(hwnd, context->tooltip.get(), context->ui);
    RETURN_IF_FAILED(context->renderer.SetUiOverlayVisible(true));
    RETURN_IF_FAILED(RenderImgViewer(context));

    if (hide_for_transition && was_visible) {
        ShowWindow(hwnd, was_zoomed ? SW_SHOWMAXIMIZED : SW_SHOW);
        UpdateWindow(hwnd);
    }

    return S_OK;
}

void SyncWindowState(HWND hwnd, UiController* ui)
{
    if (ui != nullptr) {
        ui->SetWindowState(util::IsWindowTopMost(hwnd), IsZoomed(hwnd));
    }
}

void SaveWindowSize(HWND hwnd, ImgViewerContext* context)
{
    if (context == nullptr || !context->config.remember_window_size || IsIconic(hwnd) || IsZoomed(hwnd)) {
        return;
    }

    RECT window_rect = {};
    if (!GetWindowRect(hwnd, &window_rect)) {
        return;
    }

    context->config.window_size.width = static_cast<int>(window_rect.right - window_rect.left);
    context->config.window_size.height = static_cast<int>(window_rect.bottom - window_rect.top);
    SaveImgViewerConfig(context->config);
}

bool IsImgViewerActionEnabled(const ImgViewerContext* context, ImgViewerAction action)
{
    if (action == ImgViewerAction::None) {
        return false;
    }

    if (action == ImgViewerAction::PreviousImage) {
        const ImageSequencePosition position = context != nullptr ? context->sequence.Position() : ImageSequencePosition{};
        return position.index > 1;
    }

    if (action == ImgViewerAction::NextImage) {
        const ImageSequencePosition position = context != nullptr ? context->sequence.Position() : ImageSequencePosition{};
        return position.total > 0 && position.index < position.total;
    }

    if (action == ImgViewerAction::ToggleColorPicker) {
        const D2D1_SIZE_U image_size = context != nullptr ? context->viewer.CurrentImagePixelSize() : D2D1_SIZE_U{};
        return image_size.width > 0 && image_size.height > 0;
    }

    return true;
}

void SyncActionStates(ImgViewerContext* context)
{
    if (context == nullptr) {
        return;
    }

    context->ui.SetActionEnabled(
        UiActionFromImgViewerAction(ImgViewerAction::PreviousImage),
        IsImgViewerActionEnabled(context, ImgViewerAction::PreviousImage));
    context->ui.SetActionEnabled(
        UiActionFromImgViewerAction(ImgViewerAction::NextImage),
        IsImgViewerActionEnabled(context, ImgViewerAction::NextImage));
    context->ui.SetActionEnabled(
        UiActionFromImgViewerAction(ImgViewerAction::ToggleColorPicker),
        IsImgViewerActionEnabled(context, ImgViewerAction::ToggleColorPicker));
}

void ShowImgViewerToast(HWND hwnd, ImgViewerContext* context, const wchar_t* text)
{
    if (context == nullptr) {
        return;
    }

    if (text == nullptr || text[0] == L'\0') {
        KillTimer(hwnd, kImgViewerToastTimerId);
        if (context->ui.HideToast()) {
            RenderImgViewer(context);
        }
        return;
    }

    context->ui.ShowToast(text);
    SetTimer(hwnd, kImgViewerToastTimerId, kToastDurationMs, nullptr);
    RenderImgViewer(context);
}

void ApplyWindowOpacity(HWND hwnd, int percent)
{
    if (hwnd == nullptr) {
        return;
    }

    const int clamped = ClampWindowOpacityPercent(percent);
    LONG_PTR ex_style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (clamped >= 100) {
        if ((ex_style & WS_EX_LAYERED) != 0) {
            SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex_style & ~WS_EX_LAYERED);
            RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
        }
        return;
    }

    if ((ex_style & WS_EX_LAYERED) == 0) {
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex_style | WS_EX_LAYERED);
    }
    const BYTE alpha = static_cast<BYTE>((std::max)(1, (clamped * 255 + 50) / 100));
    SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);
}

void SetImgViewerWindowOpacity(HWND hwnd, ImgViewerContext* context, int percent)
{
    if (context == nullptr) {
        return;
    }

    context->current_window_opacity_percent = ClampWindowOpacityPercent(percent);
    ApplyWindowOpacity(hwnd, context->current_window_opacity_percent);
    if (context->settings_window != nullptr && IsWindow(context->settings_window)) {
        PostMessageW(
            context->settings_window,
            kImgViewerSettingsOpacityChangedMessage,
            static_cast<WPARAM>(context->current_window_opacity_percent),
            0);
    }
}

void ExecuteImgViewerAction(HWND hwnd, ImgViewerContext* context, ImgViewerAction action)
{
    if (!IsImgViewerActionEnabled(context, action)) {
        return;
    }

    switch (action) {
    case ImgViewerAction::OpenImage:
    case ImgViewerAction::OpenMenu:
        break;
    case ImgViewerAction::OpenSettings:
        OpenImgViewerSettingsWindow(hwnd, context);
        break;
    case ImgViewerAction::PreviousImage:
        NavigateImageFile(hwnd, context, -1);
        break;
    case ImgViewerAction::NextImage:
        NavigateImageFile(hwnd, context, 1);
        break;
    case ImgViewerAction::ZoomIn:
        if (context != nullptr && context->viewer.ZoomByStep(1, context->renderer.ViewportPixelSize())) {
            RenderImgViewer(context);
        }
        break;
    case ImgViewerAction::ZoomOut:
        if (context != nullptr && context->viewer.ZoomByStep(-1, context->renderer.ViewportPixelSize())) {
            RenderImgViewer(context);
        }
        break;
    case ImgViewerAction::RotateClockwise:
        if (context != nullptr && context->viewer.RotateClockwise()) {
            RenderImgViewer(context);
        }
        break;
    case ImgViewerAction::FlipHorizontal:
        if (context != nullptr && context->viewer.FlipHorizontal()) {
            RenderImgViewer(context);
        }
        break;
    case ImgViewerAction::FlipVertical:
        if (context != nullptr && context->viewer.FlipVertical()) {
            RenderImgViewer(context);
        }
        break;
    case ImgViewerAction::ResetView:
        if (context != nullptr && context->viewer.ResetView()) {
            RenderImgViewer(context);
        }
        break;
    case ImgViewerAction::ToggleColorPicker:
        if (context != nullptr) {
            SetColorPickerActive(context, !context->color_picker_active);
            RenderImgViewer(context);
        }
        break;
    case ImgViewerAction::ToggleTopMost: {
        const bool top_most = !util::IsWindowTopMost(hwnd);
        SetWindowPos(
            hwnd,
            top_most ? HWND_TOPMOST : HWND_NOTOPMOST,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        if (context != nullptr) {
            SyncWindowState(hwnd, &context->ui);
            RenderImgViewer(context);
        }
        break;
    }
    case ImgViewerAction::Minimize:
        ShowWindow(hwnd, SW_MINIMIZE);
        break;
    case ImgViewerAction::ToggleMaximize:
        ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
        if (context != nullptr) {
            SyncWindowState(hwnd, &context->ui);
            RenderImgViewer(context);
        }
        break;
    case ImgViewerAction::Close:
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        break;
    default:
        break;
    }
}

bool HandleImgViewerColorPick(HWND hwnd, ImgViewerContext* context, D2D1_POINT_2F point)
{
    if (context == nullptr || !context->color_picker_active) {
        return false;
    }

    ImgViewerColorSample color;
    if (!context->viewer.SampleColorAt(point.x, point.y, context->renderer.ViewportPixelSize(), &color)) {
        return true;
    }

    wchar_t hex_text[8] = {};
    swprintf_s(hex_text, L"#%02X%02X%02X", color.red, color.green, color.blue);
    CopyTextToClipboard(hwnd, hex_text);
    SetColorPickerActive(context, false);
    const std::wstring toast_text = std::wstring(L"Copied ") + hex_text;
    ShowImgViewerToast(hwnd, context, toast_text.c_str());
    return true;
}

void LoadImgViewerImageFile(HWND hwnd, ImgViewerContext* context, const wchar_t* path)
{
    if (context == nullptr || path == nullptr || path[0] == L'\0') {
        return;
    }

    const HRESULT hr = context->viewer.LoadImageFile(path, context->renderer.BitmapDeviceContext());
    if (FAILED(hr)) {
        MessageBoxW(hwnd, L"Could not open the selected image.", kImgViewerWindowTitle, MB_OK | MB_ICONERROR);
        return;
    }

    const HRESULT sequence_hr = context->sequence.SetCurrentPath(path);
    if (FAILED(sequence_hr)) {
        MessageBoxW(hwnd, L"Could not read the image folder.", kImgViewerWindowTitle, MB_OK | MB_ICONWARNING);
    }
    SyncActionStates(context);
    SetColorPickerActive(context, false);

    const std::wstring file_name = util::FileNameFromPath(path, kImgViewerWindowTitle);
    const ImageSequencePosition position = context->sequence.Position();
    const D2D1_SIZE_U image_size = context->viewer.CurrentImagePixelSize();
    wchar_t position_text[64] = {};
    if (position.total > 0) {
        swprintf_s(position_text, L" (%zu/%zu)", position.index, position.total);
    }

    wchar_t resolution_text[64] = {};
    swprintf_s(resolution_text, L"  %ux%u", image_size.width, image_size.height);
    const std::wstring title_text = file_name + position_text + resolution_text;
    context->ui.SetTitleText(title_text.c_str());
    SetWindowTextW(hwnd, title_text.c_str());
    RenderImgViewer(context);
}

bool NavigateImgViewerImageFile(HWND hwnd, ImgViewerContext* context, int direction)
{
    return NavigateImageFile(hwnd, context, direction);
}

void HandleImgViewerOpenImageCommand(HWND hwnd, ImgViewerContext* context)
{
    constexpr win32::NativeFileDialogFilter filters[] = {
        {L"Images", L"*.bmp;*.dib;*.gif;*.ico;*.jpg;*.jpeg;*.jpe;*.png;*.tif;*.tiff;*.webp"},
        {L"All files", L"*.*"},
    };

    std::wstring path;
    const HRESULT hr = win32::OpenNativeFileDialog(hwnd, {filters, 1}, &path);
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        return;
    }

    if (FAILED(hr)) {
        MessageBoxW(hwnd, L"Could not show the image picker.", kImgViewerWindowTitle, MB_OK | MB_ICONERROR);
        return;
    }

    LoadImgViewerImageFile(hwnd, context, path.c_str());
}

namespace {

bool NavigateImageFile(HWND hwnd, ImgViewerContext* context, int direction)
{
    if (context == nullptr) {
        return false;
    }

    const std::optional<std::wstring> path = direction < 0 ? context->sequence.Previous() : context->sequence.Next();
    if (!path) {
        return false;
    }

    LoadImgViewerImageFile(hwnd, context, path->c_str());
    return true;
}

bool CopyTextToClipboard(HWND hwnd, const wchar_t* text)
{
    if (text == nullptr || !OpenClipboard(hwnd)) {
        return false;
    }

    auto close_clipboard = wil::scope_exit([] { CloseClipboard(); });
    EmptyClipboard();

    const size_t byte_count = (wcslen(text) + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, byte_count);
    if (memory == nullptr) {
        return false;
    }

    wchar_t* clipboard_text = static_cast<wchar_t*>(GlobalLock(memory));
    if (clipboard_text == nullptr) {
        GlobalFree(memory);
        return false;
    }

    memcpy(clipboard_text, text, byte_count);
    GlobalUnlock(memory);
    if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
        GlobalFree(memory);
        return false;
    }

    return true;
}

void SetColorPickerActive(ImgViewerContext* context, bool active)
{
    if (context == nullptr) {
        return;
    }

    context->color_picker_active = active && IsImgViewerActionEnabled(context, ImgViewerAction::ToggleColorPicker);
    context->ui.SetColorPickerActive(context->color_picker_active);
}

} // namespace
