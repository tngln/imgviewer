#pragma once

#include <windows.h>

#include <imm.h>
#include <ole2.h>
#include <UIAutomationCore.h>

#include <array>
#include <optional>
#include <string>
#include <wil/com.h>
#include <wil/resource.h>

#include "imgviewer.action.hpp"
#include "imgviewer.config.hpp"
#include "image.analysis.hpp"
#include "image.sequence.hpp"
#include "imgviewer.edit.hpp"
#include "imgviewer.interaction.hpp"
#include "imgviewer.renderer.hpp"
#include "imgviewer.viewer.hpp"
#include "ui.hpp"
#include "ui.graphics_device.hpp"
#include "ui.popup.hpp"

constexpr wchar_t kImgViewerWindowTitle[] = L"ImgViewer";

class ImgViewerUi;

struct ImgViewerContext final {
    ImgViewerContext();

    GraphicsDevice graphics_device;
    ImgViewerRenderer renderer;
    ImgViewerUi* main_ui = nullptr;
    UiController ui;
    PopupHost popup;
    ImgViewerController viewer;
    ImgViewerEditController edit;
    ImgViewerInteractionState interaction;
    ImageSequence sequence;
    ImgViewerConfig config;
    std::wstring current_image_path;
    std::optional<ImagePixelAnalysis> current_image_analysis;
    bool current_image_analysis_failed = false;
    int current_window_opacity_percent = 100;
    int current_toolbar_scale_percent = 125;
    std::array<ImgViewerAction, 256> pressed_key_actions = {};
    HIMC main_window_ime_context = nullptr;
    bool main_window_ime_enabled = true;
    wil::com_ptr<IRawElementProviderSimple> accessibility_provider;
    wil::unique_hwnd tooltip;
    HWND settings_window = nullptr;
    void* settings_context = nullptr;
    void* about_context = nullptr;
    void* developer_context = nullptr;
    bool color_picker_active = false;
    bool color_picker_has_sample = false;
    std::wstring color_picker_hex_text;
    bool current_image_from_clipboard = false;
    bool current_image_from_screenshot = false;
    bool info_panel_visible = false;
    bool interactive_size_move_active = false;
    ImgViewerAction pending_edge_click_action = ImgViewerAction::None;
    D2D1_POINT_2F pending_edge_click_point = {};
    DWORD animation_last_tick_ms = 0;
    int last_window_size_toast_width = 0;
    int last_window_size_toast_height = 0;
};

HRESULT RenderImgViewer(ImgViewerContext* context);
HRESULT ResetImgViewerUi(HWND hwnd, ImgViewerContext* context);
DWORD ImgViewerWindowStyle(bool borderless);
HRESULT ApplyImgViewerWindowFrame(HWND hwnd, ImgViewerContext* context, bool hide_for_transition);
void SyncWindowState(HWND hwnd, ImgViewerContext* context);
void SaveWindowSize(HWND hwnd, ImgViewerContext* context);
bool IsImgViewerActionEnabled(const ImgViewerContext* context, ImgViewerAction action);
void SyncActionStates(ImgViewerContext* context);
void ShowImgViewerToast(HWND hwnd, ImgViewerContext* context, const wchar_t* text);
void SyncImgViewerAnimationTimer(HWND hwnd, ImgViewerContext* context);
void SyncImgViewerMainWindowIme(HWND hwnd, ImgViewerContext* context);
void InvalidateImgViewerInfoPanelAnalysis(ImgViewerContext* context);
void ResetImgViewerTransientInput(HWND hwnd, ImgViewerContext* context);
bool EnterImgViewerEditMode(HWND hwnd, ImgViewerContext* context);
void ExitImgViewerEditMode(HWND hwnd, ImgViewerContext* context);
void SetImgViewerEditTool(HWND hwnd, ImgViewerContext* context, ImgViewerEditTool tool, const wchar_t* toast_text);
void SetImgViewerColorPickerActive(HWND hwnd, ImgViewerContext* context, bool active);
bool UpdateImgViewerColorPickerSample(ImgViewerContext* context, D2D1_POINT_2F point);
void ApplyWindowOpacity(HWND hwnd, int percent);
void SetImgViewerWindowOpacity(HWND hwnd, ImgViewerContext* context, int percent);
void SetImgViewerToolbarScale(HWND hwnd, ImgViewerContext* context, int percent);
void ExecuteImgViewerAction(HWND hwnd, ImgViewerContext* context, ImgViewerAction action);
void LoadImgViewerImageFile(HWND hwnd, ImgViewerContext* context, const wchar_t* path);
bool NavigateImgViewerImageFile(HWND hwnd, ImgViewerContext* context, int direction);
void HandleImgViewerOpenImageCommand(HWND hwnd, ImgViewerContext* context);
void HandleImgViewerCaptureRegion(HWND hwnd, ImgViewerContext* context);
void HandleImgViewerSaveImageAsCommand(HWND hwnd, ImgViewerContext* context);
void HandleImgViewerPasteClipboard(HWND hwnd, ImgViewerContext* context);
HRESULT OpenImgViewerSettingsWindow(HWND owner, ImgViewerContext* context);
void CleanupImgViewerSettingsWindow(ImgViewerContext* context, void* settings_context);
HRESULT OpenImgViewerAboutWindow(HWND owner, ImgViewerContext* context);
void CleanupImgViewerAboutWindow(ImgViewerContext* context, void* about_context);
HRESULT OpenImgViewerDeveloperWindow(HWND owner, ImgViewerContext* context);
void CleanupImgViewerDeveloperWindow(ImgViewerContext* context, void* developer_context);
