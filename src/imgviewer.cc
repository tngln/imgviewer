#include "imgviewer.hpp"

#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

#include <wil/result_macros.h>
#include <wil/resource.h>

#include "win32.dialog.hpp"
#include "win32.util.hpp"
#include "imgviewer.messages.hpp"
#include "imgviewer.about.hpp"
#include "imgviewer.settings.hpp"
#include "imgviewer.ui.action.hpp"
#include "imgviewer.ui.hpp"
#include "math.hpp"
#include "ui.tooltip.hpp"
#include "win32.clipboard.hpp"

namespace {

constexpr UINT kToastDurationMs = 2000;

bool NavigateImageFile(HWND hwnd, ImgViewerContext* context, int direction);
void SetColorPickerActive(ImgViewerContext* context, bool active);
void UpdateImgViewerInfoPanelState(ImgViewerContext* context);

std::wstring Unavailable()
{
    return L"Unavailable";
}

std::wstring FormatImageDimensions(D2D1_SIZE_U size)
{
    if (size.width == 0 || size.height == 0) {
        return L"-";
    }

    wchar_t text[64] = {};
    swprintf_s(text, L"%ux%u", size.width, size.height);
    return text;
}

std::wstring FormatSequence(ImageSequencePosition position)
{
    if (position.total == 0) {
        return L"-";
    }

    wchar_t text[64] = {};
    swprintf_s(text, L"%zu/%zu", position.index, position.total);
    return text;
}

std::wstring FormatFileSize(ULONGLONG byte_count)
{
    constexpr ULONGLONG kKiB = 1024;
    constexpr ULONGLONG kMiB = kKiB * 1024;
    constexpr ULONGLONG kGiB = kMiB * 1024;

    wchar_t text[64] = {};
    if (byte_count >= kGiB) {
        swprintf_s(text, L"%.1f GB", static_cast<double>(byte_count) / static_cast<double>(kGiB));
    } else if (byte_count >= kMiB) {
        swprintf_s(text, L"%.1f MB", static_cast<double>(byte_count) / static_cast<double>(kMiB));
    } else if (byte_count >= kKiB) {
        swprintf_s(text, L"%.1f KB", static_cast<double>(byte_count) / static_cast<double>(kKiB));
    } else {
        swprintf_s(text, L"%llu bytes", byte_count);
    }
    return text;
}

std::wstring FormatFileTime(FILETIME file_time)
{
    FILETIME local_time = {};
    SYSTEMTIME system_time = {};
    if (!FileTimeToLocalFileTime(&file_time, &local_time) || !FileTimeToSystemTime(&local_time, &system_time)) {
        return Unavailable();
    }

    wchar_t date_text[64] = {};
    wchar_t time_text[64] = {};
    if (GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &system_time, nullptr, date_text, ARRAYSIZE(date_text), nullptr) == 0 ||
        GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &system_time, nullptr, time_text, ARRAYSIZE(time_text)) == 0) {
        return Unavailable();
    }

    return std::wstring(date_text) + L" " + time_text;
}

std::wstring FormatImageType(const std::wstring& path, bool clipboard)
{
    if (clipboard) {
        return L"Clipboard image";
    }

    std::wstring extension = std::filesystem::path(path).extension().wstring();
    if (extension.empty()) {
        return Unavailable();
    }
    if (extension[0] == L'.') {
        extension.erase(extension.begin());
    }
    std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t value) {
        return static_cast<wchar_t>(std::towupper(value));
    });
    return extension;
}

std::wstring FormatZoom(const ImgViewerSnapshot& snapshot, D2D1_SIZE_U viewport_size)
{
    const float fit_scale = math::FitScale(snapshot.pixel_size, viewport_size);
    if (fit_scale <= 0.0f) {
        return L"-";
    }

    wchar_t text[64] = {};
    swprintf_s(text, L"%.0f%%", static_cast<double>(fit_scale * snapshot.zoom_multiplier * 100.0f));
    return text;
}

std::wstring FormatRotation(float rotation_degrees)
{
    wchar_t text[64] = {};
    swprintf_s(text, L"%.0f deg", static_cast<double>(rotation_degrees));
    return text;
}

std::wstring FormatFlips(const ImgViewerSnapshot& snapshot)
{
    if (snapshot.flipped_horizontal && snapshot.flipped_vertical) {
        return L"Horizontal, Vertical";
    }
    if (snapshot.flipped_horizontal) {
        return L"Horizontal";
    }
    if (snapshot.flipped_vertical) {
        return L"Vertical";
    }
    return L"None";
}

} // namespace

ImgViewerContext::ImgViewerContext() : ui(std::make_unique<ImgViewerUi>()) {}

HRESULT RenderImgViewer(ImgViewerContext* context)
{
    if (context == nullptr) {
        return S_OK;
    }

    UpdateImgViewerInfoPanelState(context);
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

    if (action == ImgViewerAction::SaveImageAs) {
        return context != nullptr && context->viewer.HasCurrentImage();
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
    context->ui.SetActionEnabled(
        UiActionFromImgViewerAction(ImgViewerAction::SaveImageAs),
        IsImgViewerActionEnabled(context, ImgViewerAction::SaveImageAs));
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

void SetImgViewerToolbarScale(HWND hwnd, ImgViewerContext* context, int percent)
{
    if (context == nullptr) {
        return;
    }

    const int clamped = ClampToolbarScalePercent(percent);
    if (context->current_toolbar_scale_percent == clamped) {
        return;
    }

    context->current_toolbar_scale_percent = clamped;
    context->ui.SetToolbarScalePercent(context->current_toolbar_scale_percent);
    if (hwnd != nullptr) {
        UpdateUiTooltipRects(hwnd, context->tooltip.get(), context->ui);
    }
    RenderImgViewer(context);
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
    case ImgViewerAction::SaveImageAs:
        HandleImgViewerSaveImageAsCommand(hwnd, context);
        break;
    case ImgViewerAction::OpenSettings:
        OpenImgViewerSettingsWindow(hwnd, context);
        break;
    case ImgViewerAction::OpenAbout:
        OpenImgViewerAboutWindow(hwnd, context);
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
    case ImgViewerAction::FitWindow:
        if (context != nullptr && context->viewer.FitWindow()) {
            RenderImgViewer(context);
        }
        break;
    case ImgViewerAction::ActualSize:
        if (context != nullptr && context->viewer.ActualSize(context->renderer.ViewportPixelSize())) {
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
    case ImgViewerAction::ToggleInfoPanel:
        if (context != nullptr) {
            context->info_panel_visible = !context->info_panel_visible;
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
    win32::CopyTextToClipboard(hwnd, hex_text);
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
    context->current_image_path = path;
    context->current_image_from_clipboard = false;
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
        {L"Images", L"*.bmp;*.dib;*.gif;*.ico;*.jpg;*.jpeg;*.jpe;*.png;*.psd;*.tif;*.tiff;*.tga;*.webp"},
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

void HandleImgViewerSaveImageAsCommand(HWND hwnd, ImgViewerContext* context)
{
    if (!IsImgViewerActionEnabled(context, ImgViewerAction::SaveImageAs)) {
        return;
    }

    constexpr win32::NativeFileDialogFilter filters[] = {
        {L"PNG image", L"*.png"},
    };

    std::wstring path;
    const HRESULT dialog_hr = win32::OpenNativeSaveFileDialog(hwnd, {filters, 1, L"png"}, &path);
    if (dialog_hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        return;
    }

    if (FAILED(dialog_hr)) {
        MessageBoxW(hwnd, L"Could not show the save dialog.", kImgViewerWindowTitle, MB_OK | MB_ICONERROR);
        return;
    }

    const HRESULT save_hr = context->viewer.SaveCurrentImagePng(path.c_str());
    if (FAILED(save_hr)) {
        MessageBoxW(hwnd, L"Could not save the image.", kImgViewerWindowTitle, MB_OK | MB_ICONERROR);
        return;
    }

    ShowImgViewerToast(hwnd, context, L"Saved image.");
}

void HandleImgViewerPasteClipboard(HWND hwnd, ImgViewerContext* context)
{
    if (context == nullptr) {
        return;
    }

    win32::ClipboardContent content;
    const HRESULT clipboard_hr = win32::ReadClipboardContent(hwnd, context->viewer.WicFactory(), &content);
    if (FAILED(clipboard_hr)) {
        ShowImgViewerToast(hwnd, context, L"Clipboard does not contain an image or path.");
        return;
    }

    if (!content.path.empty()) {
        LoadImgViewerImageFile(hwnd, context, content.path.c_str());
        return;
    }

    if (!content.bitmap_source) {
        ShowImgViewerToast(hwnd, context, L"Clipboard does not contain an image or path.");
        return;
    }

    const HRESULT load_hr = context->viewer.LoadBitmapSource(
        content.bitmap_source.get(),
        context->renderer.BitmapDeviceContext());
    if (FAILED(load_hr)) {
        ShowImgViewerToast(hwnd, context, L"Could not paste clipboard image.");
        return;
    }

    context->sequence.Clear();
    context->current_image_path.clear();
    context->current_image_from_clipboard = true;
    SyncActionStates(context);
    SetColorPickerActive(context, false);

    const D2D1_SIZE_U image_size = context->viewer.CurrentImagePixelSize();
    wchar_t title_text[96] = {};
    swprintf_s(title_text, L"<Clipboard>  %ux%u", image_size.width, image_size.height);
    context->ui.SetTitleText(title_text);
    SetWindowTextW(hwnd, title_text);
    RenderImgViewer(context);
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

void SetColorPickerActive(ImgViewerContext* context, bool active)
{
    if (context == nullptr) {
        return;
    }

    context->color_picker_active = active && IsImgViewerActionEnabled(context, ImgViewerAction::ToggleColorPicker);
    context->ui.SetColorPickerActive(context->color_picker_active);
}

void UpdateImgViewerInfoPanelState(ImgViewerContext* context)
{
    if (context == nullptr) {
        return;
    }

    ImgViewerUiInfoPanelState state;
    state.visible = context->info_panel_visible;

    const bool has_image = context->viewer.HasCurrentImage();
    const bool clipboard = context->current_image_from_clipboard;
    const ImgViewerSnapshot snapshot = context->viewer.Snapshot();
    if (!has_image) {
        state.name = L"No image";
        state.path = Unavailable();
        state.dimensions = L"-";
        state.type = Unavailable();
        state.file_size = Unavailable();
        state.modified_time = Unavailable();
        state.sequence = L"-";
        state.zoom = L"-";
        state.rotation = L"-";
        state.flips = L"-";
    } else {
        state.name = clipboard ? L"<Clipboard>" : util::FileNameFromPath(context->current_image_path.c_str(), L"-");
        state.path = clipboard || context->current_image_path.empty() ? Unavailable() : context->current_image_path;
        state.dimensions = FormatImageDimensions(snapshot.pixel_size);
        state.type = FormatImageType(context->current_image_path, clipboard);
        state.file_size = Unavailable();
        state.modified_time = Unavailable();
        state.sequence = FormatSequence(context->sequence.Position());
        state.zoom = FormatZoom(snapshot, context->renderer.ViewportPixelSize());
        state.rotation = FormatRotation(snapshot.rotation_degrees);
        state.flips = FormatFlips(snapshot);

        WIN32_FILE_ATTRIBUTE_DATA attributes = {};
        if (!clipboard &&
            !context->current_image_path.empty() &&
            GetFileAttributesExW(context->current_image_path.c_str(), GetFileExInfoStandard, &attributes) &&
            (attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            const ULONGLONG file_size =
                (static_cast<ULONGLONG>(attributes.nFileSizeHigh) << 32) |
                static_cast<ULONGLONG>(attributes.nFileSizeLow);
            state.file_size = FormatFileSize(file_size);
            state.modified_time = FormatFileTime(attributes.ftLastWriteTime);
        }
    }

    static_cast<ImgViewerUi*>(context->ui.Root())->SetInfoPanelState(std::move(state));
}

} // namespace
