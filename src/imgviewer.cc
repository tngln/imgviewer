#include "imgviewer.hpp"

#include <optional>
#include <string>
#include <utility>

#include <wil/result_macros.h>
#include <wil/resource.h>

#include "imgviewer.about.hpp"
#include "imgviewer.messages.hpp"
#include "imgviewer.settings.hpp"
#include "imgviewer.ui.action.hpp"
#include "imgviewer.ui.hpp"
#include "ui.tooltip.hpp"
#include "util.format.hpp"
#include "win32.clipboard.hpp"
#include "win32.dialog.hpp"
#include "win32.screen_capture.hpp"
#include "win32.util.hpp"

namespace {

constexpr UINT kToastDurationMs = 2000;
constexpr UINT kAnimationTickMs = 16;

bool NavigateImageFile(HWND hwnd, ImgViewerContext* context, int direction);
bool HasCurrentImageFilePath(const ImgViewerContext* context);
bool EnsureEditDocument(ImgViewerContext* context);
void EnsureInfoPanelAnalysis(ImgViewerContext* context);
void InvalidateInfoPanelAnalysis(ImgViewerContext* context);
void UpdateImgViewerInfoPanelState(ImgViewerContext* context);
HRESULT LoadImgViewerScreenshotBitmap(HWND hwnd, ImgViewerContext* context, IWICBitmapSource* source);

} // namespace

ImgViewerContext::ImgViewerContext() : ui(std::make_unique<ImgViewerUi>()) {}

HRESULT RenderImgViewer(ImgViewerContext* context)
{
    if (context == nullptr) {
        return S_OK;
    }

    UpdateImgViewerInfoPanelState(context);
    static_cast<ImgViewerUi*>(context->ui.Root())->SetAnimationState(context->viewer.AnimationState());
    static_cast<ImgViewerUi*>(context->ui.Root())->SetEditToolbarState(ImgViewerUiEditToolbarState{
        .visible = context->edit.Active(),
        .tool = context->edit.Tool(),
        .dirty = context->edit.Dirty(),
        .can_undo = context->edit.CanUndo(),
        .can_redo = context->edit.CanRedo(),
    });
    static_cast<ImgViewerUi*>(context->ui.Root())->SetSelectionToolstripState(ImgViewerUiSelectionToolstripState{
        .visible = context->edit.Active() && context->edit.HasPixelSelection(),
    });
    RETURN_IF_FAILED(context->renderer.Render(context->viewer, context->edit, context->ui));
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
    if (context != nullptr && context->config.remember_window_size &&
        util::CaptureWindowSize(hwnd, &context->config.window_size.width, &context->config.window_size.height)) {
        SaveImgViewerConfig(context->config);
    }
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

    if (action == ImgViewerAction::ToggleEditMode ||
        action == ImgViewerAction::EditSelect ||
        action == ImgViewerAction::EditPixelSelect ||
        action == ImgViewerAction::EditPen ||
        action == ImgViewerAction::EditText ||
        action == ImgViewerAction::EditCrop ||
        action == ImgViewerAction::EditCopySelection ||
        action == ImgViewerAction::EditMosaicSelection ||
        action == ImgViewerAction::EditRotateClockwise ||
        action == ImgViewerAction::EditUndo ||
        action == ImgViewerAction::EditRedo) {
        if (context == nullptr || !context->viewer.HasCurrentImage()) {
            return false;
        }
        if (action == ImgViewerAction::EditCopySelection || action == ImgViewerAction::EditMosaicSelection) {
            return context->edit.HasPixelSelection();
        }
        return true;
    }

    if (action == ImgViewerAction::ShowInFileExplorer) {
        return HasCurrentImageFilePath(context);
    }

    if (action == ImgViewerAction::ToggleAnimationLoop ||
        action == ImgViewerAction::ToggleAnimationPlayback ||
        action == ImgViewerAction::PreviousAnimationFrame ||
        action == ImgViewerAction::NextAnimationFrame) {
        return context != nullptr && context->viewer.HasAnimation();
    }

    return true;
}

void SyncActionStates(ImgViewerContext* context)
{
    if (context == nullptr) {
        return;
    }

    static constexpr std::array kSyncActions{
        ImgViewerAction::PreviousImage,
        ImgViewerAction::NextImage,
        ImgViewerAction::ToggleColorPicker,
        ImgViewerAction::SaveImageAs,
        ImgViewerAction::ShowInFileExplorer,
        ImgViewerAction::ToggleAnimationLoop,
        ImgViewerAction::ToggleAnimationPlayback,
        ImgViewerAction::PreviousAnimationFrame,
        ImgViewerAction::NextAnimationFrame,
    };
    for (const ImgViewerAction action : kSyncActions) {
        context->ui.SetActionEnabled(
            UiActionFromImgViewerAction(action),
            IsImgViewerActionEnabled(context, action));
    }
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

void SyncImgViewerAnimationTimer(HWND hwnd, ImgViewerContext* context)
{
    if (hwnd == nullptr || context == nullptr) {
        return;
    }

    const ImgViewerAnimationState state = context->viewer.AnimationState();
    if (state.available && state.playing) {
        context->animation_last_tick_ms = GetTickCount();
        SetTimer(hwnd, kImgViewerAnimationTimerId, kAnimationTickMs, nullptr);
        return;
    }

    KillTimer(hwnd, kImgViewerAnimationTimerId);
    context->animation_last_tick_ms = 0;
}

void ResetImgViewerTransientInput(HWND hwnd, ImgViewerContext* context)
{
    if (context == nullptr) {
        return;
    }

    context->edit.CancelTransientTool();
    context->viewer.CancelTransientViewGesture();
    context->interaction.ResetTransientInput();
    if (hwnd != nullptr && GetCapture() == hwnd) {
        ReleaseCapture();
    }
}

bool EnterImgViewerEditMode(HWND hwnd, ImgViewerContext* context)
{
    if (context == nullptr || !EnsureEditDocument(context)) {
        return false;
    }

    ResetImgViewerTransientInput(hwnd, context);
    context->color_picker_active = false;
    context->ui.SetColorPickerActive(false);
    context->edit.SetActive(true);
    context->interaction.EnterEditing();
    if (context->viewer.AnimationState().playing) {
        context->viewer.ToggleAnimationPlayback();
        SyncImgViewerAnimationTimer(hwnd, context);
    }
    ShowImgViewerToast(hwnd, context, L"Edit mode on.");
    return true;
}

void ExitImgViewerEditMode(HWND hwnd, ImgViewerContext* context)
{
    if (context == nullptr) {
        return;
    }

    ResetImgViewerTransientInput(hwnd, context);
    context->edit.SetActive(false);
    context->interaction.EnterViewing();
    ShowImgViewerToast(hwnd, context, L"Edit mode off.");
}

void SetImgViewerEditTool(HWND hwnd, ImgViewerContext* context, ImgViewerEditTool tool, const wchar_t* toast_text)
{
    if (context == nullptr || !EnsureEditDocument(context)) {
        return;
    }

    ResetImgViewerTransientInput(hwnd, context);
    context->color_picker_active = false;
    context->ui.SetColorPickerActive(false);
    context->edit.SetTool(tool);
    context->edit.SetActive(true);
    context->interaction.EnterEditing();
    if (context->viewer.AnimationState().playing) {
        context->viewer.ToggleAnimationPlayback();
        SyncImgViewerAnimationTimer(hwnd, context);
    }
    if (toast_text != nullptr) {
        ShowImgViewerToast(hwnd, context, toast_text);
    } else {
        RenderImgViewer(context);
    }
}

void SetImgViewerColorPickerActive(HWND hwnd, ImgViewerContext* context, bool active)
{
    if (context == nullptr) {
        return;
    }

    ResetImgViewerTransientInput(hwnd, context);
    const bool enabled = active && IsImgViewerActionEnabled(context, ImgViewerAction::ToggleColorPicker);
    if (enabled) {
        context->edit.SetActive(false);
        context->interaction.BeginColorPick();
    } else {
        context->interaction.EndColorPick();
    }
    context->color_picker_active = enabled;
    context->ui.SetColorPickerActive(context->color_picker_active);
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

inline void TryViewerAction(ImgViewerContext* context, bool success)
{
    if (success) {
        RenderImgViewer(context);
    }
}

inline void TryEditAction(ImgViewerContext* context, bool success)
{
    if (success) {
        RenderImgViewer(context);
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
    case ImgViewerAction::CaptureDesktop:
        HandleImgViewerCaptureDesktop(hwnd, context);
        break;
    case ImgViewerAction::CaptureRegion:
        HandleImgViewerCaptureRegion(hwnd, context);
        break;
    case ImgViewerAction::SaveImageAs:
        HandleImgViewerSaveImageAsCommand(hwnd, context);
        break;
    case ImgViewerAction::ShowInFileExplorer:
        if (context != nullptr && FAILED(util::ShowFileInExplorer(context->current_image_path.c_str()))) {
            ShowImgViewerToast(hwnd, context, L"Could not show file in Explorer.");
        }
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
        if (context != nullptr) TryViewerAction(context, context->viewer.ZoomByStep(1, context->renderer.ViewportPixelSize()));
        break;
    case ImgViewerAction::ZoomOut:
        if (context != nullptr) TryViewerAction(context, context->viewer.ZoomByStep(-1, context->renderer.ViewportPixelSize()));
        break;
    case ImgViewerAction::FitWindow:
        if (context != nullptr) TryViewerAction(context, context->viewer.FitWindow());
        break;
    case ImgViewerAction::ActualSize:
        if (context != nullptr) TryViewerAction(context, context->viewer.ActualSize(context->renderer.ViewportPixelSize()));
        break;
    case ImgViewerAction::RotateClockwise:
        if (context != nullptr) TryViewerAction(context, context->viewer.RotateClockwise());
        break;
    case ImgViewerAction::FlipHorizontal:
        if (context != nullptr) TryViewerAction(context, context->viewer.FlipHorizontal());
        break;
    case ImgViewerAction::FlipVertical:
        if (context != nullptr) TryViewerAction(context, context->viewer.FlipVertical());
        break;
    case ImgViewerAction::ResetView:
        if (context != nullptr) TryViewerAction(context, context->viewer.ResetView());
        break;
    case ImgViewerAction::ToggleColorPicker:
        if (context != nullptr) {
            SetImgViewerColorPickerActive(hwnd, context, !context->color_picker_active);
            RenderImgViewer(context);
        }
        break;
    case ImgViewerAction::ToggleEditMode:
        if (context != nullptr) {
            if (context->edit.Active()) {
                ExitImgViewerEditMode(hwnd, context);
            } else {
                EnterImgViewerEditMode(hwnd, context);
            }
        }
        break;
    case ImgViewerAction::EditSelect:
        SetImgViewerEditTool(hwnd, context, ImgViewerEditTool::Select, L"Edit select.");
        break;
    case ImgViewerAction::EditPixelSelect:
        SetImgViewerEditTool(hwnd, context, ImgViewerEditTool::PixelSelect, L"Pixel select.");
        break;
    case ImgViewerAction::EditPen:
        SetImgViewerEditTool(hwnd, context, ImgViewerEditTool::Pen, L"Edit pen.");
        break;
    case ImgViewerAction::EditText:
        SetImgViewerEditTool(hwnd, context, ImgViewerEditTool::Text, L"Edit text.");
        break;
    case ImgViewerAction::EditCrop:
        SetImgViewerEditTool(hwnd, context, ImgViewerEditTool::Crop, L"Edit crop.");
        break;
    case ImgViewerAction::EditCopySelection:
        if (context != nullptr) {
            wil::com_ptr<IWICBitmapSource> selected_pixels;
            if (SUCCEEDED(context->edit.CopySelectedPixels(context->viewer.WicFactory(), selected_pixels.put())) &&
                SUCCEEDED(win32::CopyBitmapSourceToClipboard(hwnd, context->viewer.WicFactory(), selected_pixels.get()))) {
                ShowImgViewerToast(hwnd, context, L"Copied selected pixels.");
            } else {
                ShowImgViewerToast(hwnd, context, L"Could not copy selected pixels.");
            }
        }
        break;
    case ImgViewerAction::EditMosaicSelection:
        if (context != nullptr) TryEditAction(context, context->edit.MosaicSelection());
        break;
    case ImgViewerAction::EditRotateClockwise:
        if (context != nullptr && EnsureEditDocument(context)) TryEditAction(context, context->edit.RotateClockwise());
        break;
    case ImgViewerAction::EditUndo:
        if (context != nullptr) TryEditAction(context, context->edit.Undo());
        break;
    case ImgViewerAction::EditRedo:
        if (context != nullptr) TryEditAction(context, context->edit.Redo());
        break;
    case ImgViewerAction::ToggleInfoPanel:
        if (context != nullptr) {
            context->info_panel_visible = !context->info_panel_visible;
            EnsureInfoPanelAnalysis(context);
            RenderImgViewer(context);
        }
        break;
    case ImgViewerAction::ToggleAnimationLoop:
        if (context != nullptr && context->viewer.ToggleAnimationLoop()) {
            RenderImgViewer(context);
        }
        break;
    case ImgViewerAction::ToggleAnimationPlayback:
        if (context != nullptr && context->viewer.ToggleAnimationPlayback()) {
            SyncImgViewerAnimationTimer(hwnd, context);
            RenderImgViewer(context);
        }
        break;
    case ImgViewerAction::PreviousAnimationFrame:
        if (context != nullptr && context->viewer.StepAnimationFrame(-1)) {
            SyncImgViewerAnimationTimer(hwnd, context);
            InvalidateInfoPanelAnalysis(context);
            RenderImgViewer(context);
        }
        break;
    case ImgViewerAction::NextAnimationFrame:
        if (context != nullptr && context->viewer.StepAnimationFrame(1)) {
            SyncImgViewerAnimationTimer(hwnd, context);
            InvalidateInfoPanelAnalysis(context);
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
    SetImgViewerColorPickerActive(hwnd, context, false);
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
    context->current_image_from_screenshot = false;
    context->edit.Clear();
    context->interaction.EnterViewing();
    InvalidateInfoPanelAnalysis(context);
    SyncActionStates(context);
    SetImgViewerColorPickerActive(hwnd, context, false);

    const std::wstring file_name = util::FileNameFromPath(path, kImgViewerWindowTitle);
    const ImageSequencePosition position = context->sequence.Position();
    const D2D1_SIZE_U image_size = context->viewer.CurrentImagePixelSize();
    wchar_t position_text[64] = {};
    if (position.total > 0) {
        swprintf_s(position_text, L" (%zu/%zu)", position.index, position.total);
    }
    const std::wstring title_text = file_name + position_text + L"  " + util::FormatImageDimensions(image_size);
    context->ui.SetTitleText(title_text.c_str());
    SetWindowTextW(hwnd, title_text.c_str());
    SyncImgViewerAnimationTimer(hwnd, context);
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

void HandleImgViewerCaptureDesktop(HWND hwnd, ImgViewerContext* context)
{
    if (context == nullptr) {
        return;
    }
    ResetImgViewerTransientInput(hwnd, context);
    context->interaction.SetModal(ImgViewerModalOwner::ScreenCapture);
    auto clear_modal = wil::scope_exit([&] {
        context->interaction.ClearModal(ImgViewerModalOwner::ScreenCapture);
    });

    const bool was_visible = IsWindowVisible(hwnd) != FALSE;
    const bool was_iconic = IsIconic(hwnd) != FALSE;
    const bool was_zoomed = IsZoomed(hwnd) != FALSE;
    if (was_visible) {
        ShowWindow(hwnd, SW_HIDE);
        Sleep(120);
    }
    bool restored = false;
    const auto restore_window = [&] {
        if (restored) {
            return;
        }
        restored = true;
        if (!was_visible) {
            return;
        }
        ShowWindow(hwnd, was_iconic ? SW_SHOWMINIMIZED : (was_zoomed ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL));
        SetForegroundWindow(hwnd);
    };

    wil::com_ptr<IWICBitmapSource> screenshot;
    const HRESULT capture_hr = win32::CaptureVirtualDesktop(context->viewer.WicFactory(), screenshot.put());
    restore_window();
    if (FAILED(capture_hr)) {
        ShowImgViewerToast(hwnd, context, L"Could not capture desktop.");
        return;
    }

    if (FAILED(LoadImgViewerScreenshotBitmap(hwnd, context, screenshot.get()))) {
        ShowImgViewerToast(hwnd, context, L"Could not capture desktop.");
    }
}

void HandleImgViewerCaptureRegion(HWND hwnd, ImgViewerContext* context)
{
    if (context == nullptr) {
        return;
    }
    ResetImgViewerTransientInput(hwnd, context);
    context->interaction.SetModal(ImgViewerModalOwner::ScreenCapture);
    auto clear_modal = wil::scope_exit([&] {
        context->interaction.ClearModal(ImgViewerModalOwner::ScreenCapture);
    });

    const bool was_visible = IsWindowVisible(hwnd) != FALSE;
    const bool was_iconic = IsIconic(hwnd) != FALSE;
    const bool was_zoomed = IsZoomed(hwnd) != FALSE;
    if (was_visible) {
        ShowWindow(hwnd, SW_HIDE);
        Sleep(120);
    }
    bool restored = false;
    const auto restore_window = [&] {
        if (restored) {
            return;
        }
        restored = true;
        if (!was_visible) {
            return;
        }
        ShowWindow(hwnd, was_iconic ? SW_SHOWMINIMIZED : (was_zoomed ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL));
        SetForegroundWindow(hwnd);
    };

    RECT region = {};
    const HRESULT select_hr = win32::SelectCaptureRegion(hwnd, &region);
    if (select_hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        restore_window();
        return;
    }
    if (FAILED(select_hr)) {
        restore_window();
        ShowImgViewerToast(hwnd, context, L"Could not capture region.");
        return;
    }

    Sleep(80);
    wil::com_ptr<IWICBitmapSource> screenshot;
    const HRESULT capture_hr = win32::CaptureScreenRect(context->viewer.WicFactory(), region, screenshot.put());
    restore_window();
    if (FAILED(capture_hr)) {
        ShowImgViewerToast(hwnd, context, L"Could not capture region.");
        return;
    }

    if (FAILED(LoadImgViewerScreenshotBitmap(hwnd, context, screenshot.get()))) {
        ShowImgViewerToast(hwnd, context, L"Could not capture region.");
    }
}

namespace {

HRESULT LoadImgViewerScreenshotBitmap(HWND hwnd, ImgViewerContext* context, IWICBitmapSource* source)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, context);
    RETURN_HR_IF_NULL(E_INVALIDARG, source);
    RETURN_IF_FAILED(context->viewer.LoadBitmapSource(source, context->renderer.BitmapDeviceContext()));

    context->sequence.Clear();
    context->current_image_path.clear();
    context->current_image_from_clipboard = false;
    context->current_image_from_screenshot = true;
    context->edit.Clear();
    context->interaction.EnterViewing();
    InvalidateInfoPanelAnalysis(context);
    SyncActionStates(context);
    SetImgViewerColorPickerActive(hwnd, context, false);

    const D2D1_SIZE_U image_size = context->viewer.CurrentImagePixelSize();
    const std::wstring title_text = L"<Screenshot>  " + util::FormatImageDimensions(image_size);
    context->ui.SetTitleText(title_text.c_str());
    SetWindowTextW(hwnd, title_text.c_str());
    SyncImgViewerAnimationTimer(hwnd, context);
    RETURN_IF_FAILED(RenderImgViewer(context));
    return S_OK;
}

} // namespace

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

    if (context->edit.HasDocument()) {
        wil::com_ptr<IWICBitmapSource> edited_source;
        const HRESULT export_hr = context->edit.ExportPngSource(context->viewer.WicFactory(), edited_source.put());
        ImageEncoder encoder;
        if (FAILED(export_hr) ||
            FAILED(encoder.Initialize(context->viewer.WicFactory())) ||
            FAILED(encoder.SavePngFile(edited_source.get(), path.c_str()))) {
            MessageBoxW(hwnd, L"Could not save the image.", kImgViewerWindowTitle, MB_OK | MB_ICONERROR);
            return;
        }
        context->edit.MarkSaved();
    } else {
        const HRESULT save_hr = context->viewer.SaveCurrentImagePng(path.c_str());
        if (FAILED(save_hr)) {
            MessageBoxW(hwnd, L"Could not save the image.", kImgViewerWindowTitle, MB_OK | MB_ICONERROR);
            return;
        }
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
    context->current_image_from_screenshot = false;
    context->edit.Clear();
    context->interaction.EnterViewing();
    InvalidateInfoPanelAnalysis(context);
    SyncActionStates(context);
    SetImgViewerColorPickerActive(hwnd, context, false);

    const D2D1_SIZE_U image_size = context->viewer.CurrentImagePixelSize();
    const std::wstring title_text = L"<Clipboard>  " + util::FormatImageDimensions(image_size);
    context->ui.SetTitleText(title_text.c_str());
    SetWindowTextW(hwnd, title_text.c_str());
    SyncImgViewerAnimationTimer(hwnd, context);
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

bool HasCurrentImageFilePath(const ImgViewerContext* context)
{
    if (context == nullptr || context->current_image_path.empty()) {
        return false;
    }

    const DWORD attributes = GetFileAttributesW(context->current_image_path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool EnsureEditDocument(ImgViewerContext* context)
{
    if (context == nullptr || !context->viewer.HasCurrentImage()) {
        return false;
    }

    if (context->edit.HasDocument()) {
        return true;
    }

    return SUCCEEDED(context->edit.Begin(context->viewer.CurrentPixelSource(), context->viewer.CurrentImagePixelSize()));
}

void EnsureInfoPanelAnalysis(ImgViewerContext* context)
{
    if (context == nullptr ||
        !context->info_panel_visible ||
        !context->viewer.HasCurrentImage() ||
        context->current_image_analysis.has_value() ||
        context->current_image_analysis_failed) {
        return;
    }

    ImagePixelAnalysis analysis;
    if (SUCCEEDED(context->viewer.AnalyzeCurrentImage(&analysis))) {
        context->current_image_analysis = analysis;
        context->current_image_analysis_failed = false;
        return;
    }

    context->current_image_analysis.reset();
    context->current_image_analysis_failed = true;
}

void InvalidateInfoPanelAnalysis(ImgViewerContext* context)
{
    if (context == nullptr) {
        return;
    }

    context->current_image_analysis.reset();
    context->current_image_analysis_failed = false;
}

void UpdateImgViewerInfoPanelState(ImgViewerContext* context)
{
    if (context == nullptr) {
        return;
    }

    ImgViewerUiInfoPanelState state;
    state.visible = context->info_panel_visible;
    EnsureInfoPanelAnalysis(context);

    const bool has_image = context->viewer.HasCurrentImage();
    const bool clipboard = context->current_image_from_clipboard;
    const bool screenshot = context->current_image_from_screenshot;
    const ImgViewerSnapshot snapshot = context->viewer.Snapshot();
    if (!has_image) {
        state.name = L"No image";
        state.path = L"Unavailable";
        state.dimensions = L"-";
        state.type = L"Unavailable";
        state.file_size = L"Unavailable";
        state.modified_time = L"Unavailable";
    } else {
        state.name = clipboard ? L"<Clipboard>" : (screenshot ? L"<Screenshot>" : util::FileNameFromPath(context->current_image_path.c_str(), L"-"));
        state.path = clipboard || screenshot || context->current_image_path.empty() ? L"Unavailable" : context->current_image_path;
        state.dimensions = util::FormatImageDimensions(snapshot.pixel_size);
        state.type = screenshot ? L"Screenshot image" : util::FormatImageType(context->current_image_path, clipboard);
        state.file_size = L"Unavailable";
        state.modified_time = L"Unavailable";
        state.exif_rows = context->viewer.CurrentImageMetadata().exif_rows;
        if (context->current_image_analysis.has_value()) {
            state.has_analysis = true;
            state.analysis = *context->current_image_analysis;
        } else if (context->current_image_analysis_failed) {
            state.analysis_unavailable = true;
        }

        WIN32_FILE_ATTRIBUTE_DATA attributes = {};
        if (!clipboard &&
            !screenshot &&
            !context->current_image_path.empty() &&
            GetFileAttributesExW(context->current_image_path.c_str(), GetFileExInfoStandard, &attributes) &&
            (attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            const ULONGLONG file_size =
                (static_cast<ULONGLONG>(attributes.nFileSizeHigh) << 32) |
                static_cast<ULONGLONG>(attributes.nFileSizeLow);
            state.file_size = util::FormatFileSize(file_size);
            state.modified_time = util::FormatFileTime(attributes.ftLastWriteTime);
        }
    }

    static_cast<ImgViewerUi*>(context->ui.Root())->SetInfoPanelState(std::move(state));
}

} // namespace

void InvalidateImgViewerInfoPanelAnalysis(ImgViewerContext* context)
{
    InvalidateInfoPanelAnalysis(context);
}
