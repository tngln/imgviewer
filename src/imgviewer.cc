#include "imgviewer.hpp"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <wil/result_macros.h>
#include <wil/resource.h>

#include "imgviewer.about.hpp"
#include "imgviewer.developer.hpp"
#include "imgviewer.messages.hpp"
#include "imgviewer.palette.hpp"
#include "imgviewer.settings.hpp"
#include "imgviewer.strings.hpp"
#include "imgviewer.ui.action.hpp"
#include "imgviewer.ui.hpp"
#include "ui.host_popup.hpp"
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
void ClearImgViewerColorPickerState(ImgViewerContext* context);
void EnsureInfoPanelAnalysis(ImgViewerContext* context);
void InvalidateInfoPanelAnalysis(ImgViewerContext* context);
void UpdateImgViewerInfoPanelState(ImgViewerContext* context);
HRESULT LoadImgViewerScreenshotBitmap(HWND hwnd, ImgViewerContext* context, IWICBitmapSource* source);

std::unique_ptr<ImgViewerUi> CreateMainUi(ImgViewerUi** main_ui)
{
    auto ui = std::make_unique<ImgViewerUi>();
    if (main_ui != nullptr) {
        *main_ui = ui.get();
    }
    return ui;
}

} // namespace

void ApplyInitialImageView(ImgViewerContext* context);

void ResetAfterImageLoad(HWND hwnd, ImgViewerContext* context)
{
    context->edit.Clear();
    context->interaction.EnterViewing();
    ApplyInitialImageView(context);
    InvalidateInfoPanelAnalysis(context);
    SyncActionStates(context);
    SetImgViewerColorPickerActive(hwnd, context, false);
}

void FinishImgViewerImageLoad(HWND hwnd, ImgViewerContext* context, const std::wstring& title_text)
{
    context->main_ui->SetTitleText(title_text.c_str());
    SetWindowTextW(hwnd, title_text.c_str());
    SyncImgViewerAnimationTimer(hwnd, context);
    InvalidateRect(hwnd, nullptr, FALSE);
}

ImgViewerContext::ImgViewerContext() : ui(CreateMainUi(&main_ui)) {}

HRESULT ResetImgViewerUi(HWND hwnd, ImgViewerContext* context)
{
    RETURN_HR_IF_NULL(E_POINTER, context);

    std::wstring title_text;
    if (hwnd != nullptr) {
        const int title_length = GetWindowTextLengthW(hwnd);
        if (title_length > 0) {
            title_text.resize(static_cast<size_t>(title_length) + 1);
            GetWindowTextW(hwnd, title_text.data(), title_length + 1);
            title_text.resize(static_cast<size_t>(title_length));
        }
    }

    ClosePopupIfOpen(&context->popup);
    context->ui.ResetRoot(CreateMainUi(&context->main_ui));
    context->info_panel_key_valid = false;
    context->main_ui->SetTitleText(title_text.empty() ? kImgViewerWindowTitle : title_text.c_str());
    context->main_ui->SetToolbarScalePercent(context->current_toolbar_scale_percent);
    SyncActionStates(context);
    if (hwnd != nullptr) {
        SyncWindowState(hwnd, context);
        InvalidateRect(hwnd, nullptr, FALSE);

        context->tooltip.reset();
        HWND tooltip = nullptr;
        RETURN_IF_FAILED(InitializeUiTooltips(hwnd, &tooltip, context->ui));
        context->tooltip.reset(tooltip);
    }
    return S_OK;
}

namespace {

uint64_t ComputeImgViewerInfoPanelKey(const ImgViewerContext* context)
{
    const ImgViewerSnapshot snapshot = context->viewer.Snapshot();
    uint64_t key = 1469598103934665603ull;
    const auto mix = [&key](uint64_t value) {
        key = (key ^ value) * 1099511628211ull;
    };
    mix(context->info_panel_visible ? 1u : 0u);
    mix(context->viewer.HasCurrentImage() ? 1u : 0u);
    mix(snapshot.pixel_size.width);
    mix(snapshot.pixel_size.height);
    mix(context->current_image_from_clipboard ? 1u : 0u);
    mix(context->current_image_from_screenshot ? 1u : 0u);
    mix(context->current_image_analysis.has_value() ? 1u : 0u);
    mix(context->current_image_analysis_failed ? 1u : 0u);
    mix(static_cast<uint64_t>(std::hash<std::wstring>{}(context->current_image_path)));
    return key;
}

} // namespace

HRESULT RenderImgViewer(ImgViewerContext* context)
{
    if (context == nullptr) {
        return S_OK;
    }

    // Info-panel content depends only on the image/analysis/visibility (not on
    // pan/zoom/cursor), so rebuild its heavy state (strings, metadata vectors,
    // file stat) only when that source key changes.
    const uint64_t info_panel_key = ComputeImgViewerInfoPanelKey(context);
    if (!context->info_panel_key_valid || info_panel_key != context->last_info_panel_key) {
        UpdateImgViewerInfoPanelState(context);
        context->last_info_panel_key = info_panel_key;
        context->info_panel_key_valid = true;
    }
    context->main_ui->SetAnimationState(context->viewer.AnimationState());
    context->main_ui->SetEditToolbarState(ImgViewerUiEditToolbarState{
        .visible = context->edit.Active(),
        .tool = context->edit.Tool(),
        .dirty = context->edit.Dirty(),
        .can_undo = context->edit.CanUndo(),
        .can_redo = context->edit.CanRedo(),
    });
    context->main_ui->SetPenToolstripState(ImgViewerUiPenToolstripState{
        .visible = context->edit.Active() && context->edit.Tool() == ImgViewerEditTool::Pen,
        .color = context->edit.PenColor(),
        .width = context->edit.PenWidth(),
    });
    context->main_ui->SetShapeToolstripState(ImgViewerUiShapeToolstripState{
        .visible = context->edit.Active() && context->edit.Tool() == ImgViewerEditTool::Shape,
        .kind = context->edit.ShapeKind(),
        .color = context->edit.PenColor(),
    });
    context->main_ui->SetTextToolstripState(ImgViewerUiTextToolstripState{
        .visible = context->edit.Active() && context->edit.Tool() == ImgViewerEditTool::Text,
        .style = context->edit.TextStyle(),
    });
    context->main_ui->SetSelectionToolstripState(ImgViewerUiSelectionToolstripState{
        .visible = context->edit.Active() && context->edit.HasPixelSelection(),
    });
    context->main_ui->SetColorPickerToolstripState(ImgViewerUiColorPickerToolstripState{
        .visible = context->color_picker_active,
        .has_sample = context->color_picker_has_sample,
        .hex_text = context->color_picker_hex_text,
    });
    RETURN_IF_FAILED(context->renderer.Render(context->viewer, context->edit, context->ui));
    return S_OK;
}

bool IsImgViewerActionEnabled(const ImgViewerContext* context, UiAction action)
{
    if (action == kUiActionNone) {
        return false;
    }

    const ImgViewerAction verb = static_cast<ImgViewerAction>(action.value);

    if (verb == ImgViewerAction::PreviousImage) {
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
        verb == ImgViewerAction::EditSelect ||
        verb == ImgViewerAction::EditPixelSelect ||
        verb == ImgViewerAction::EditPen ||
        verb == ImgViewerAction::EditSetPenColor ||
        verb == ImgViewerAction::EditSetPenWidth ||
        verb == ImgViewerAction::EditShape ||
        verb == ImgViewerAction::EditSetShapeKind ||
        verb == ImgViewerAction::EditText ||
        verb == ImgViewerAction::EditTextFontChanged ||
        verb == ImgViewerAction::EditSetTextFontSize ||
        verb == ImgViewerAction::EditSetTextColor ||
        verb == ImgViewerAction::EditSetTextBackground ||
        verb == ImgViewerAction::EditCrop ||
        verb == ImgViewerAction::EditCancelCrop ||
        verb == ImgViewerAction::EditCopySelection ||
        verb == ImgViewerAction::EditMosaicSelection ||
        verb == ImgViewerAction::EditDeleteSelection ||
        verb == ImgViewerAction::EditRotateClockwise ||
        verb == ImgViewerAction::EditUndo ||
        verb == ImgViewerAction::EditRedo) {
        if (context == nullptr || !context->viewer.HasCurrentImage()) {
            return false;
        }
        if (verb == ImgViewerAction::EditCopySelection || verb == ImgViewerAction::EditMosaicSelection) {
            return context->edit.HasPixelSelection();
        }
        if (verb == ImgViewerAction::EditDeleteSelection) {
            return context->edit.Active() && context->edit.Tool() == ImgViewerEditTool::Select && context->edit.HasSelection();
        }
        return true;
    }

    if (verb == ImgViewerAction::ShowInFileExplorer) {
        return HasCurrentImageFilePath(context);
    }

    if (verb == ImgViewerAction::ToggleAnimationLoop ||
        verb == ImgViewerAction::ToggleAnimationPlayback ||
        verb == ImgViewerAction::PreviousAnimationFrame ||
        verb == ImgViewerAction::NextAnimationFrame) {
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
        context->main_ui->SetActionEnabled(
            UiAction(action),
            IsImgViewerActionEnabled(context, UiAction(action)));
    }
}

void ShowImgViewerToast(HWND hwnd, ImgViewerContext* context, const wchar_t* text)
{
    if (context == nullptr) {
        return;
    }

    if (text == nullptr || text[0] == L'\0') {
        KillTimer(hwnd, kImgViewerToastTimerId);
        if (context->main_ui->HideToast()) {
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return;
    }

    context->main_ui->ShowToast(text);
    SetTimer(hwnd, kImgViewerToastTimerId, kToastDurationMs, nullptr);
    InvalidateRect(hwnd, nullptr, FALSE);
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
    context->pending_edge_click_action = ImgViewerAction::None;
    context->pending_edge_click_point = {};
    context->interaction.ResetTransientInput();
    if (hwnd != nullptr && GetCapture() == hwnd) {
        ReleaseCapture();
    }
    SyncImgViewerMainWindowIme(hwnd, context);
}

bool CommitImgViewerCropAndCenter(HWND, ImgViewerContext* context)
{
    if (context == nullptr) {
        return false;
    }

    const bool committed = context->edit.CommitCropSession();
    if (!context->edit.HasCrop()) {
        return committed;
    }

    const D2D1_RECT_F crop_rect = context->edit.CropRect();
    const D2D1_POINT_2F crop_center = D2D1::Point2F(
        (crop_rect.left + crop_rect.right) * 0.5f,
        (crop_rect.top + crop_rect.bottom) * 0.5f);
    return context->viewer.CenterOnImagePoint(crop_center) || committed;
}

bool EnterImgViewerEditMode(HWND hwnd, ImgViewerContext* context)
{
    if (context == nullptr || !EnsureEditDocument(context)) {
        return false;
    }

    ResetImgViewerTransientInput(hwnd, context);
    ClearImgViewerColorPickerState(context);
    context->edit.SetActive(true);
    context->interaction.EnterEditing();
    if (context->viewer.AnimationState().playing) {
        context->viewer.ToggleAnimationPlayback();
        SyncImgViewerAnimationTimer(hwnd, context);
    }
    ShowImgViewerToast(hwnd, context, ImgViewerString(ImgViewerStringId::EditModeOn));
    SyncImgViewerMainWindowIme(hwnd, context);
    return true;
}

void ExitImgViewerEditMode(HWND hwnd, ImgViewerContext* context)
{
    if (context == nullptr) {
        return;
    }

    CommitImgViewerCropAndCenter(hwnd, context);
    ResetImgViewerTransientInput(hwnd, context);
    context->edit.SetActive(false);
    context->interaction.EnterViewing();
    ShowImgViewerToast(hwnd, context, ImgViewerString(ImgViewerStringId::EditModeOff));
    SyncImgViewerMainWindowIme(hwnd, context);
}

void SetImgViewerEditTool(HWND hwnd, ImgViewerContext* context, ImgViewerEditTool tool, const wchar_t* toast_text)
{
    if (context == nullptr || !EnsureEditDocument(context)) {
        return;
    }

    if (context->edit.Tool() == ImgViewerEditTool::Crop && tool != ImgViewerEditTool::Crop) {
        CommitImgViewerCropAndCenter(hwnd, context);
    }
    ResetImgViewerTransientInput(hwnd, context);
    ClearImgViewerColorPickerState(context);
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
        InvalidateRect(hwnd, nullptr, FALSE);
    }
    SyncImgViewerMainWindowIme(hwnd, context);
}

void SetImgViewerColorPickerActive(HWND hwnd, ImgViewerContext* context, bool active)
{
    if (context == nullptr) {
        return;
    }

    ResetImgViewerTransientInput(hwnd, context);
    const bool enabled = active && IsImgViewerActionEnabled(context, UiAction(ImgViewerAction::ToggleColorPicker));
    if (enabled) {
        context->edit.SetActive(false);
        context->interaction.BeginColorPick();
    } else {
        context->interaction.EndColorPick();
    }
    if (enabled) {
        context->color_picker_active = true;
        context->color_picker_has_sample = false;
        context->color_picker_hex_text.clear();
        context->main_ui->SetColorPickerToolstripState(ImgViewerUiColorPickerToolstripState{
            .visible = true,
            .has_sample = false,
        });
    } else {
        ClearImgViewerColorPickerState(context);
    }
    SyncImgViewerMainWindowIme(hwnd, context);
}

bool UpdateImgViewerColorPickerSample(ImgViewerContext* context, D2D1_POINT_2F point)
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
    context->color_picker_has_sample = true;
    context->color_picker_hex_text = hex_text;
    context->main_ui->SetColorPickerToolstripState(ImgViewerUiColorPickerToolstripState{
        .visible = context->color_picker_active,
        .has_sample = context->color_picker_has_sample,
        .hex_text = context->color_picker_hex_text,
    });
    return true;
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
    context->main_ui->SetToolbarScalePercent(context->current_toolbar_scale_percent);
    if (hwnd != nullptr) {
        UpdateUiTooltipRects(hwnd, context->tooltip.get(), context->ui);
    }
    InvalidateRect(hwnd, nullptr, FALSE);
}

void DoAction(HWND hwnd, ImgViewerContext* /*context*/, bool success)
{
    if (success) {
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

void ApplyInitialImageView(ImgViewerContext* context)
{
    if (context == nullptr || !context->viewer.HasCurrentImage()) {
        return;
    }

    switch (context->config.initial_image_view_mode) {
    case InitialImageViewMode::ActualSize:
        if (context->viewer.ActualSize(context->renderer.ViewportPixelSize())) {
            return;
        }
        [[fallthrough]];
    case InitialImageViewMode::FitWindow:
    default:
        context->viewer.FitWindow();
        break;
    }
}

void SetPenColor(HWND hwnd, ImgViewerContext* context, D2D1_COLOR_F color)
{
    if (context == nullptr || !EnsureEditDocument(context)) {
        return;
    }

    if (context->edit.Tool() == ImgViewerEditTool::Crop) {
        CommitImgViewerCropAndCenter(hwnd, context);
    }
    ResetImgViewerTransientInput(hwnd, context);
    ClearImgViewerColorPickerState(context);
    if (context->edit.Tool() != ImgViewerEditTool::Shape) {
        context->edit.SetTool(ImgViewerEditTool::Pen);
    }
    context->edit.SetActive(true);
    context->edit.SetPenColor(color);
    context->interaction.EnterEditing();
    if (context->viewer.AnimationState().playing) {
        context->viewer.ToggleAnimationPlayback();
        SyncImgViewerAnimationTimer(hwnd, context);
    }
    InvalidateRect(hwnd, nullptr, FALSE);
}

void SetPenWidth(HWND hwnd, ImgViewerContext* context, float width)
{
    if (context == nullptr || !EnsureEditDocument(context)) {
        return;
    }

    if (context->edit.Tool() == ImgViewerEditTool::Crop) {
        CommitImgViewerCropAndCenter(hwnd, context);
    }
    ResetImgViewerTransientInput(hwnd, context);
    ClearImgViewerColorPickerState(context);
    context->edit.SetTool(ImgViewerEditTool::Pen);
    context->edit.SetActive(true);
    context->edit.SetPenWidth(width);
    context->interaction.EnterEditing();
    if (context->viewer.AnimationState().playing) {
        context->viewer.ToggleAnimationPlayback();
        SyncImgViewerAnimationTimer(hwnd, context);
    }
    InvalidateRect(hwnd, nullptr, FALSE);
}

void SetShapeKind(HWND hwnd, ImgViewerContext* context, ImgViewerShapeKind kind)
{
    if (context == nullptr || !EnsureEditDocument(context)) {
        return;
    }

    if (context->edit.Tool() == ImgViewerEditTool::Crop) {
        CommitImgViewerCropAndCenter(hwnd, context);
    }
    ResetImgViewerTransientInput(hwnd, context);
    ClearImgViewerColorPickerState(context);
    context->edit.SetTool(ImgViewerEditTool::Shape);
    context->edit.SetActive(true);
    context->edit.SetShapeKind(kind);
    context->interaction.EnterEditing();
    if (context->viewer.AnimationState().playing) {
        context->viewer.ToggleAnimationPlayback();
        SyncImgViewerAnimationTimer(hwnd, context);
    }
    InvalidateRect(hwnd, nullptr, FALSE);
}

void PrepareTextStyleEdit(HWND hwnd, ImgViewerContext* context)
{
    if (context == nullptr || !EnsureEditDocument(context)) {
        return;
    }

    if (context->edit.IsEditingText()) {
        context->viewer.CancelTransientViewGesture();
        context->interaction.ClearPointerCapture();
        if (hwnd != nullptr && GetCapture() == hwnd) {
            ReleaseCapture();
        }
    } else {
        if (context->edit.Tool() == ImgViewerEditTool::Crop) {
            CommitImgViewerCropAndCenter(hwnd, context);
        }
        ResetImgViewerTransientInput(hwnd, context);
    }
    ClearImgViewerColorPickerState(context);
    if (!context->edit.IsEditingText()) {
        context->edit.SetTool(ImgViewerEditTool::Text);
    }
    context->edit.SetActive(true);
    context->interaction.EnterEditing();
    if (context->viewer.AnimationState().playing) {
        context->viewer.ToggleAnimationPlayback();
        SyncImgViewerAnimationTimer(hwnd, context);
    }
}

void SetTextFontFamily(HWND hwnd, ImgViewerContext* context, std::wstring font_family)
{
    PrepareTextStyleEdit(hwnd, context);
    if (context == nullptr || !context->edit.Active()) {
        return;
    }
    context->edit.SetTextFontFamily(std::move(font_family));
    InvalidateRect(hwnd, nullptr, FALSE);
}

void SetTextFontSize(HWND hwnd, ImgViewerContext* context, float font_size)
{
    PrepareTextStyleEdit(hwnd, context);
    if (context == nullptr || !context->edit.Active()) {
        return;
    }
    context->edit.SetTextFontSize(font_size);
    InvalidateRect(hwnd, nullptr, FALSE);
}

void SetTextColor(HWND hwnd, ImgViewerContext* context, D2D1_COLOR_F color)
{
    PrepareTextStyleEdit(hwnd, context);
    if (context == nullptr || !context->edit.Active()) {
        return;
    }
    context->edit.SetTextColor(color);
    InvalidateRect(hwnd, nullptr, FALSE);
}

void SetTextBackground(HWND hwnd, ImgViewerContext* context, D2D1_COLOR_F color, bool has_background)
{
    PrepareTextStyleEdit(hwnd, context);
    if (context == nullptr || !context->edit.Active()) {
        return;
    }
    context->edit.SetTextBackground(color, has_background);
    InvalidateRect(hwnd, nullptr, FALSE);
}

void ExecuteImgViewerAction(HWND hwnd, ImgViewerContext* context, UiAction action)
{
    if (!IsImgViewerActionEnabled(context, action)) {
        return;
    }

    const ImgViewerAction verb = static_cast<ImgViewerAction>(action.value);

    switch (verb) {
    case ImgViewerAction::OpenImage:
    case ImgViewerAction::OpenMenu:
        break;
    case ImgViewerAction::CaptureRegion:
        HandleImgViewerCaptureRegion(hwnd, context);
        break;
    case ImgViewerAction::SaveImageAs:
        HandleImgViewerSaveImageAsCommand(hwnd, context);
        break;
    case ImgViewerAction::ShowInFileExplorer:
        if (context != nullptr && FAILED(util::ShowFileInExplorer(context->current_image_path.c_str()))) {
            ShowImgViewerToast(hwnd, context, ImgViewerString(ImgViewerStringId::CouldNotShowFileInExplorer));
        }
        break;
    case ImgViewerAction::OpenSettings:
        OpenImgViewerSettingsWindow(hwnd, context);
        break;
#if defined(IMGVIEWER_ENABLE_DEVELOPER_WINDOW)
    case ImgViewerAction::OpenDeveloper:
        OpenImgViewerDeveloperWindow(hwnd, context);
        break;
#endif
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
        if (context != nullptr) DoAction(hwnd, context, context->viewer.ZoomByStep(1, context->renderer.ViewportPixelSize()));
        break;
    case ImgViewerAction::ZoomOut:
        if (context != nullptr) DoAction(hwnd, context, context->viewer.ZoomByStep(-1, context->renderer.ViewportPixelSize()));
        break;
    case ImgViewerAction::FitWindow:
        if (context != nullptr) DoAction(hwnd, context, context->viewer.FitWindow());
        break;
    case ImgViewerAction::ActualSize:
        if (context != nullptr) DoAction(hwnd, context, context->viewer.ActualSize(context->renderer.ViewportPixelSize()));
        break;
    case ImgViewerAction::RotateClockwise:
        if (context != nullptr) DoAction(hwnd, context, context->viewer.RotateClockwise());
        break;
    case ImgViewerAction::FlipHorizontal:
        if (context != nullptr) DoAction(hwnd, context, context->viewer.FlipHorizontal());
        break;
    case ImgViewerAction::FlipVertical:
        if (context != nullptr) DoAction(hwnd, context, context->viewer.FlipVertical());
        break;
    case ImgViewerAction::ResetView:
        if (context != nullptr) DoAction(hwnd, context, context->viewer.ResetView());
        break;
    case ImgViewerAction::ToggleColorPicker:
        if (context != nullptr) {
            SetImgViewerColorPickerActive(hwnd, context, !context->color_picker_active);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        break;
    case ImgViewerAction::CopyColorPickerValue:
        if (context != nullptr && context->color_picker_has_sample && !context->color_picker_hex_text.empty()) {
            if (win32::CopyTextToClipboard(hwnd, context->color_picker_hex_text.c_str())) {
                const std::wstring toast_text = std::wstring(L"Copied ") + context->color_picker_hex_text;
                ShowImgViewerToast(hwnd, context, toast_text.c_str());
            } else {
                ShowImgViewerToast(hwnd, context, ImgViewerString(ImgViewerStringId::CouldNotCopyColor));
            }
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
    case ImgViewerAction::EditSetPenColor:
        SetPenColor(hwnd, context, UnpackColor(action.arg));
        break;
    case ImgViewerAction::EditSetPenWidth:
        SetPenWidth(hwnd, context, UnpackFloat(action.arg));
        break;
    case ImgViewerAction::EditShape:
        SetImgViewerEditTool(hwnd, context, ImgViewerEditTool::Shape, L"Edit shape.");
        break;
    case ImgViewerAction::EditSetShapeKind:
        SetShapeKind(hwnd, context, static_cast<ImgViewerShapeKind>(action.arg));
        break;
    case ImgViewerAction::EditText:
        SetImgViewerEditTool(hwnd, context, ImgViewerEditTool::Text, L"Edit text.");
        break;
    case ImgViewerAction::EditTextFontChanged:
        if (context != nullptr && context->ui.Root() != nullptr) {
            SetTextFontFamily(hwnd, context, context->main_ui->SelectedTextFontFamily());
        }
        break;
    case ImgViewerAction::EditSetTextFontSize:
        SetTextFontSize(hwnd, context, UnpackFloat(action.arg));
        break;
    case ImgViewerAction::EditSetTextColor:
        SetTextColor(hwnd, context, UnpackColor(action.arg));
        break;
    case ImgViewerAction::EditSetTextBackground: {
        const bool has_bg = (action.arg & 1) != 0;
        const D2D1_COLOR_F bg_color = has_bg ? UnpackColor(action.arg >> 8) : D2D1::ColorF(D2D1::ColorF::Yellow, 0.0f);
        SetTextBackground(hwnd, context, bg_color, has_bg);
        break;
    }
    case ImgViewerAction::EditCrop:
        SetImgViewerEditTool(hwnd, context, ImgViewerEditTool::Crop, L"Edit crop.");
        break;
    case ImgViewerAction::EditCancelCrop:
        if (context != nullptr && context->edit.CancelCropSession()) {
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        break;
    case ImgViewerAction::EditCopySelection:
        if (context != nullptr) {
            wil::com_ptr<IWICBitmapSource> selected_pixels;
            if (SUCCEEDED(context->edit.CopySelectedPixels(context->viewer.WicFactory(), selected_pixels.put())) &&
                SUCCEEDED(win32::CopyBitmapSourceToClipboard(hwnd, context->viewer.WicFactory(), selected_pixels.get()))) {
                ShowImgViewerToast(hwnd, context, ImgViewerString(ImgViewerStringId::CopiedSelectedPixels));
            } else {
                ShowImgViewerToast(hwnd, context, ImgViewerString(ImgViewerStringId::CouldNotCopySelectedPixels));
            }
        }
        break;
    case ImgViewerAction::EditMosaicSelection:
        if (context != nullptr) DoAction(hwnd, context, context->edit.MosaicSelection());
        break;
    case ImgViewerAction::EditDeleteSelection:
        if (context != nullptr) DoAction(hwnd, context, context->edit.DeleteSelection());
        break;
    case ImgViewerAction::EditRotateClockwise:
        if (context != nullptr && EnsureEditDocument(context)) DoAction(hwnd, context, context->edit.RotateClockwise());
        break;
    case ImgViewerAction::EditUndo:
        if (context != nullptr) DoAction(hwnd, context, context->edit.Undo());
        break;
    case ImgViewerAction::EditRedo:
        if (context != nullptr) DoAction(hwnd, context, context->edit.Redo());
        break;
    case ImgViewerAction::ToggleInfoPanel:
        if (context != nullptr) {
            context->info_panel_visible = !context->info_panel_visible;
            EnsureInfoPanelAnalysis(context);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        break;
    case ImgViewerAction::ToggleAnimationLoop:
        if (context != nullptr && context->viewer.ToggleAnimationLoop()) {
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        break;
    case ImgViewerAction::ToggleAnimationPlayback:
        if (context != nullptr && context->viewer.ToggleAnimationPlayback()) {
            SyncImgViewerAnimationTimer(hwnd, context);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        break;
    case ImgViewerAction::PreviousAnimationFrame:
        if (context != nullptr && context->viewer.StepAnimationFrame(-1)) {
            SyncImgViewerAnimationTimer(hwnd, context);
            InvalidateInfoPanelAnalysis(context);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        break;
    case ImgViewerAction::NextAnimationFrame:
        if (context != nullptr && context->viewer.StepAnimationFrame(1)) {
            SyncImgViewerAnimationTimer(hwnd, context);
            InvalidateInfoPanelAnalysis(context);
            InvalidateRect(hwnd, nullptr, FALSE);
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
            SyncWindowState(hwnd, context);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        break;
    }
    case ImgViewerAction::Minimize:
        ShowWindow(hwnd, SW_MINIMIZE);
        break;
    case ImgViewerAction::ToggleMaximize:
        ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
        if (context != nullptr) {
            SyncWindowState(hwnd, context);
            InvalidateRect(hwnd, nullptr, FALSE);
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

    const HRESULT hr = context->viewer.LoadImageFile(path, context->graphics_device.D2DContext());
    if (FAILED(hr)) {
        MessageBoxW(hwnd, ImgViewerString(ImgViewerStringId::CouldNotOpenSelectedImage), kImgViewerWindowTitle, MB_OK | MB_ICONERROR);
        return;
    }

    const HRESULT sequence_hr = context->sequence.SetCurrentPath(path);
    if (FAILED(sequence_hr)) {
        MessageBoxW(hwnd, ImgViewerString(ImgViewerStringId::CouldNotReadImageFolder), kImgViewerWindowTitle, MB_OK | MB_ICONWARNING);
    }
    context->current_image_path = path;
    context->current_image_from_clipboard = false;
    context->current_image_from_screenshot = false;
    ResetAfterImageLoad(hwnd, context);

    const std::wstring file_name = util::FileNameFromPath(path, kImgViewerWindowTitle);
    const ImageSequencePosition position = context->sequence.Position();
    const D2D1_SIZE_U image_size = context->viewer.CurrentImagePixelSize();
    wchar_t position_text[64] = {};
    if (position.total > 0) {
        swprintf_s(position_text, L" (%zu/%zu)", position.index, position.total);
    }
    const std::wstring title_text = file_name + position_text + L"  " + util::FormatImageDimensions(image_size);
    FinishImgViewerImageLoad(hwnd, context, title_text);
}

bool NavigateImgViewerImageFile(HWND hwnd, ImgViewerContext* context, int direction)
{
    return NavigateImageFile(hwnd, context, direction);
}

void HandleImgViewerOpenImageCommand(HWND hwnd, ImgViewerContext* context)
{
    const win32::NativeFileDialogFilter filters[] = {
        {ImgViewerString(ImgViewerStringId::ImagesFilter), L"*.bmp;*.dib;*.gif;*.ico;*.jpg;*.jpeg;*.jpe;*.png;*.psd;*.tif;*.tiff;*.tga;*.webp"},
        {ImgViewerString(ImgViewerStringId::AllFilesFilter), L"*.*"},
    };

    std::wstring path;
    const HRESULT hr = win32::OpenNativeFileDialog(hwnd, {filters, 1}, &path);
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        return;
    }

    if (FAILED(hr)) {
        MessageBoxW(hwnd, ImgViewerString(ImgViewerStringId::CouldNotShowImagePicker), kImgViewerWindowTitle, MB_OK | MB_ICONERROR);
        return;
    }

    LoadImgViewerImageFile(hwnd, context, path.c_str());
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
        ShowImgViewerToast(hwnd, context, ImgViewerString(ImgViewerStringId::CouldNotCaptureRegion));
        return;
    }

    Sleep(80);
    wil::com_ptr<IWICBitmapSource> screenshot;
    const HRESULT capture_hr = win32::CaptureScreenRect(context->viewer.WicFactory(), region, screenshot.put());
    restore_window();
    if (FAILED(capture_hr)) {
        ShowImgViewerToast(hwnd, context, ImgViewerString(ImgViewerStringId::CouldNotCaptureRegion));
        return;
    }

    if (FAILED(LoadImgViewerScreenshotBitmap(hwnd, context, screenshot.get()))) {
        ShowImgViewerToast(hwnd, context, ImgViewerString(ImgViewerStringId::CouldNotCaptureRegion));
    }
}

namespace {

HRESULT LoadImgViewerScreenshotBitmap(HWND hwnd, ImgViewerContext* context, IWICBitmapSource* source)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, context);
    RETURN_HR_IF_NULL(E_INVALIDARG, source);
    RETURN_IF_FAILED(context->viewer.LoadBitmapSource(source, context->graphics_device.D2DContext()));

    context->sequence.Clear();
    context->current_image_path.clear();
    context->current_image_from_clipboard = false;
    context->current_image_from_screenshot = true;
    ResetAfterImageLoad(hwnd, context);

    const D2D1_SIZE_U image_size = context->viewer.CurrentImagePixelSize();
    const std::wstring title_text = L"<Screenshot>  " + util::FormatImageDimensions(image_size);
    FinishImgViewerImageLoad(hwnd, context, title_text);
    return S_OK;
}

} // namespace

void HandleImgViewerSaveImageAsCommand(HWND hwnd, ImgViewerContext* context)
{
    if (!IsImgViewerActionEnabled(context, UiAction(ImgViewerAction::SaveImageAs))) {
        return;
    }

    const win32::NativeFileDialogFilter filters[] = {
        {ImgViewerString(ImgViewerStringId::PngImageFilter), L"*.png"},
    };

    std::wstring path;
    const HRESULT dialog_hr = win32::OpenNativeSaveFileDialog(hwnd, {filters, 1, L"png"}, &path);
    if (dialog_hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        return;
    }

    if (FAILED(dialog_hr)) {
        MessageBoxW(hwnd, ImgViewerString(ImgViewerStringId::CouldNotShowSaveDialog), kImgViewerWindowTitle, MB_OK | MB_ICONERROR);
        return;
    }

    if (context->edit.HasDocument()) {
        CommitImgViewerCropAndCenter(hwnd, context);
        context->edit.CommitTextEditSession();
        wil::com_ptr<IWICBitmapSource> edited_source;
        const HRESULT export_hr = context->edit.ExportPngSource(
            context->viewer.WicFactory(),
            &context->graphics_device,
            edited_source.put());
        ImageEncoder encoder;
        if (FAILED(export_hr) ||
            FAILED(encoder.Initialize(context->viewer.WicFactory())) ||
            FAILED(encoder.SavePngFile(edited_source.get(), path.c_str()))) {
            MessageBoxW(hwnd, ImgViewerString(ImgViewerStringId::CouldNotSaveImage), kImgViewerWindowTitle, MB_OK | MB_ICONERROR);
            return;
        }
        const bool exported_hdr_source = context->edit.SourceIsHdr();
        context->edit.MarkSaved();
        ShowImgViewerToast(
            hwnd,
            context,
            ImgViewerString(exported_hdr_source ? ImgViewerStringId::SavedSdrAnnotationExportHdrSourcePreserved : ImgViewerStringId::SavedImage));
        return;
    } else {
        const bool exported_hdr_source = context->viewer.CurrentImageMetadata().color_info.dynamic_range.high_dynamic_range;
        const HRESULT save_hr = context->viewer.SaveCurrentImagePng(path.c_str());
        if (FAILED(save_hr)) {
            MessageBoxW(hwnd, ImgViewerString(ImgViewerStringId::CouldNotSaveImage), kImgViewerWindowTitle, MB_OK | MB_ICONERROR);
            return;
        }
        if (exported_hdr_source) {
            ShowImgViewerToast(hwnd, context, ImgViewerString(ImgViewerStringId::SavedSdrCopyHdrSourcePreserved));
            return;
        }
    }

    ShowImgViewerToast(hwnd, context, ImgViewerString(ImgViewerStringId::SavedImage));
}

void HandleImgViewerPasteClipboard(HWND hwnd, ImgViewerContext* context)
{
    if (context == nullptr) {
        return;
    }

    win32::ClipboardContent content;
    const HRESULT clipboard_hr = win32::ReadClipboardContent(hwnd, context->viewer.WicFactory(), &content);
    if (FAILED(clipboard_hr)) {
        ShowImgViewerToast(hwnd, context, ImgViewerString(ImgViewerStringId::ClipboardDoesNotContainImageOrPath));
        return;
    }

    if (!content.path.empty()) {
        LoadImgViewerImageFile(hwnd, context, content.path.c_str());
        return;
    }

    if (!content.bitmap_source) {
        ShowImgViewerToast(hwnd, context, ImgViewerString(ImgViewerStringId::ClipboardDoesNotContainImageOrPath));
        return;
    }

    const HRESULT load_hr = context->viewer.LoadBitmapSource(
        content.bitmap_source.get(),
        context->graphics_device.D2DContext());
    if (FAILED(load_hr)) {
        ShowImgViewerToast(hwnd, context, ImgViewerString(ImgViewerStringId::CouldNotPasteClipboardImage));
        return;
    }

    context->sequence.Clear();
    context->current_image_path.clear();
    context->current_image_from_clipboard = true;
    context->current_image_from_screenshot = false;
    ResetAfterImageLoad(hwnd, context);

    const D2D1_SIZE_U image_size = context->viewer.CurrentImagePixelSize();
    const std::wstring title_text = L"<Clipboard>  " + util::FormatImageDimensions(image_size);
    FinishImgViewerImageLoad(hwnd, context, title_text);
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

    return SUCCEEDED(context->edit.Begin(
        context->viewer.CurrentPixelSource(),
        context->viewer.CurrentImagePixelSize(),
        context->viewer.CurrentImageMetadata(),
        context->current_image_path));
}

void ClearImgViewerColorPickerState(ImgViewerContext* context)
{
    if (context == nullptr) {
        return;
    }

    context->color_picker_active = false;
    context->color_picker_has_sample = false;
    context->color_picker_hex_text.clear();
    context->main_ui->SetColorPickerToolstripState(ImgViewerUiColorPickerToolstripState{
        .visible = false,
        .has_sample = false,
    });
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
        state.name = ImgViewerString(ImgViewerStringId::NoImage);
        state.path = ImgViewerString(ImgViewerStringId::Unavailable);
        state.dimensions = L"-";
        state.type = ImgViewerString(ImgViewerStringId::Unavailable);
        state.file_size = ImgViewerString(ImgViewerStringId::Unavailable);
        state.modified_time = ImgViewerString(ImgViewerStringId::Unavailable);
    } else {
        state.name = clipboard ? L"<Clipboard>" : (screenshot ? L"<Screenshot>" : util::FileNameFromPath(context->current_image_path.c_str(), L"-"));
        state.path = clipboard || screenshot || context->current_image_path.empty() ? ImgViewerString(ImgViewerStringId::Unavailable) : context->current_image_path;
        state.dimensions = util::FormatImageDimensions(snapshot.pixel_size);
        state.type = screenshot ? ImgViewerString(ImgViewerStringId::ScreenshotImage) : util::FormatImageType(context->current_image_path, clipboard);
        state.file_size = ImgViewerString(ImgViewerStringId::Unavailable);
        state.modified_time = ImgViewerString(ImgViewerStringId::Unavailable);
        state.color_rows = context->viewer.CurrentImageMetadata().color_rows;
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

    context->main_ui->SetInfoPanelState(std::move(state));
}

} // namespace

void InvalidateImgViewerInfoPanelAnalysis(ImgViewerContext* context)
{
    InvalidateInfoPanelAnalysis(context);
}
