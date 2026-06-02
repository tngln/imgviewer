#include "app.hpp"

#include <optional>
#include <string>

#include <wil/result_macros.h>

#include "path.util.hpp"
#include "win32.dialog.hpp"
#include "win32.util.hpp"

namespace {

bool NavigateImageFile(HWND hwnd, AppContext* context, int direction);

} // namespace

HRESULT RenderApplication(AppContext* context)
{
    if (context == nullptr) {
        return S_OK;
    }

    RETURN_IF_FAILED(context->renderer.Render(context->viewer, context->ui));
    return S_OK;
}

void SyncWindowState(HWND hwnd, UiController* ui)
{
    if (ui != nullptr) {
        ui->SetWindowState(util::IsWindowTopMost(hwnd), IsZoomed(hwnd));
    }
}

void SaveWindowSize(HWND hwnd, AppContext* context)
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
    SaveAppConfig(context->config);
}

bool IsAppActionEnabled(const AppContext* context, AppAction action)
{
    if (action == AppAction::None) {
        return false;
    }

    if (action == AppAction::PreviousImage) {
        const ImageSequencePosition position = context != nullptr ? context->sequence.Position() : ImageSequencePosition{};
        return position.index > 1;
    }

    if (action == AppAction::NextImage) {
        const ImageSequencePosition position = context != nullptr ? context->sequence.Position() : ImageSequencePosition{};
        return position.total > 0 && position.index < position.total;
    }

    return true;
}

void SyncActionStates(AppContext* context)
{
    if (context == nullptr) {
        return;
    }

    context->ui.SetActionEnabled(AppAction::PreviousImage, IsAppActionEnabled(context, AppAction::PreviousImage));
    context->ui.SetActionEnabled(AppAction::NextImage, IsAppActionEnabled(context, AppAction::NextImage));
}

void ExecuteAppAction(HWND hwnd, AppContext* context, AppAction action)
{
    if (!IsAppActionEnabled(context, action)) {
        return;
    }

    switch (action) {
    case AppAction::OpenImage:
        break;
    case AppAction::PreviousImage:
        NavigateImageFile(hwnd, context, -1);
        break;
    case AppAction::NextImage:
        NavigateImageFile(hwnd, context, 1);
        break;
    case AppAction::ZoomIn:
        if (context != nullptr && context->viewer.ZoomByStep(1, context->renderer.ViewportPixelSize())) {
            RenderApplication(context);
        }
        break;
    case AppAction::ZoomOut:
        if (context != nullptr && context->viewer.ZoomByStep(-1, context->renderer.ViewportPixelSize())) {
            RenderApplication(context);
        }
        break;
    case AppAction::RotateClockwise:
        if (context != nullptr && context->viewer.RotateClockwise()) {
            RenderApplication(context);
        }
        break;
    case AppAction::FlipHorizontal:
        if (context != nullptr && context->viewer.FlipHorizontal()) {
            RenderApplication(context);
        }
        break;
    case AppAction::FlipVertical:
        if (context != nullptr && context->viewer.FlipVertical()) {
            RenderApplication(context);
        }
        break;
    case AppAction::ResetView:
        if (context != nullptr && context->viewer.ResetView()) {
            RenderApplication(context);
        }
        break;
    case AppAction::ToggleTopMost: {
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
            RenderApplication(context);
        }
        break;
    }
    case AppAction::Minimize:
        ShowWindow(hwnd, SW_MINIMIZE);
        break;
    case AppAction::ToggleMaximize:
        ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
        if (context != nullptr) {
            SyncWindowState(hwnd, &context->ui);
            RenderApplication(context);
        }
        break;
    case AppAction::Close:
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        break;
    default:
        break;
    }
}

void LoadAppImageFile(HWND hwnd, AppContext* context, const wchar_t* path)
{
    if (context == nullptr || path == nullptr || path[0] == L'\0') {
        return;
    }

    const HRESULT hr = context->viewer.LoadImageFile(path, context->renderer.BitmapDeviceContext());
    if (FAILED(hr)) {
        MessageBoxW(hwnd, L"Could not open the selected image.", kAppWindowTitle, MB_OK | MB_ICONERROR);
        return;
    }

    const HRESULT sequence_hr = context->sequence.SetCurrentPath(path);
    if (FAILED(sequence_hr)) {
        MessageBoxW(hwnd, L"Could not read the image folder.", kAppWindowTitle, MB_OK | MB_ICONWARNING);
    }
    SyncActionStates(context);

    const std::wstring file_name = util::FileNameFromPath(path, kAppWindowTitle);
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
    RenderApplication(context);
}

bool NavigateAppImageFile(HWND hwnd, AppContext* context, int direction)
{
    return NavigateImageFile(hwnd, context, direction);
}

void HandleOpenImageCommand(HWND hwnd, AppContext* context)
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
        MessageBoxW(hwnd, L"Could not show the image picker.", kAppWindowTitle, MB_OK | MB_ICONERROR);
        return;
    }

    LoadAppImageFile(hwnd, context, path.c_str());
}

namespace {

bool NavigateImageFile(HWND hwnd, AppContext* context, int direction)
{
    if (context == nullptr) {
        return false;
    }

    const std::optional<std::wstring> path = direction < 0 ? context->sequence.Previous() : context->sequence.Next();
    if (!path) {
        return false;
    }

    LoadAppImageFile(hwnd, context, path->c_str());
    return true;
}

} // namespace
