#include "imgviewer.hpp"

#include <optional>
#include <string>

#include <wil/result_macros.h>

#include "win32.dialog.hpp"
#include "win32.util.hpp"
#include "imgviewer.settings.hpp"

namespace {

bool NavigateImageFile(HWND hwnd, ImgViewerContext* context, int direction);

} // namespace

HRESULT RenderImgViewer(ImgViewerContext* context)
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

    return true;
}

void SyncActionStates(ImgViewerContext* context)
{
    if (context == nullptr) {
        return;
    }

    context->ui.SetActionEnabled(ImgViewerAction::PreviousImage, IsImgViewerActionEnabled(context, ImgViewerAction::PreviousImage));
    context->ui.SetActionEnabled(ImgViewerAction::NextImage, IsImgViewerActionEnabled(context, ImgViewerAction::NextImage));
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

} // namespace
