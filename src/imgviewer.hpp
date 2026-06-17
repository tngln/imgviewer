#pragma once

#include <windows.h>

#include <imm.h>
#include <ole2.h>

#include <array>
#include <memory>
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
#include "ui.graphics_device.hpp"
#include "ui.popup.hpp"

constexpr wchar_t kImgViewerWindowTitle[] = L"ImgViewer";

class ImgViewerUi;
class UiWindowDelegate;

namespace imgviewer {
class ScriptEngine;
}

struct ImgViewerContext final {
    ImgViewerContext();
    ~ImgViewerContext();

    GraphicsDevice graphics_device;
    ImgViewerRenderer renderer;
    std::unique_ptr<imgviewer::ScriptEngine> script_engine;
    std::unique_ptr<ScriptView> ui;
    ImgViewerUi* main_ui = nullptr;
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
    HWND settings_window = nullptr;
    UiWindowDelegate* settings_context = nullptr;
    UiWindowDelegate* about_context = nullptr;
    UiWindowDelegate* developer_context = nullptr;
    bool color_picker_active = false;
    bool color_picker_has_sample = false;
    std::wstring color_picker_hex_text;
    bool current_image_from_clipboard = false;
    bool current_image_from_screenshot = false;
    bool info_panel_visible = false;
    bool interactive_size_move_active = false;
    DWORD animation_last_tick_ms = 0;
    int last_window_size_toast_width = 0;
    int last_window_size_toast_height = 0;
    // Gating for the per-frame info-panel state rebuild (which formats strings,
    // copies metadata vectors, and stats the file). Rebuilt only when its source
    // key changes; reset by ResetImgViewerUi so a fresh UI re-pushes.
    uint64_t last_info_panel_key = 0;
    bool info_panel_key_valid = false;
};

HRESULT RenderImgViewer(ImgViewerContext* context);
HRESULT ResetImgViewerUi(HWND hwnd, ImgViewerContext* context);
DWORD ImgViewerMainWindowStyle(bool borderless);
HRESULT ApplyImgViewerWindowFrame(HWND hwnd, ImgViewerContext* context, bool hide_for_transition);
void SyncWindowState(HWND hwnd, ImgViewerContext* context);
void SaveWindowSize(HWND hwnd, ImgViewerContext* context);
bool IsImgViewerActionEnabled(const ImgViewerContext* context, UiAction action);
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
void ExecuteImgViewerAction(HWND hwnd, ImgViewerContext* context, UiAction action);
void LoadImgViewerImageFile(HWND hwnd, ImgViewerContext* context, const wchar_t* path);
bool NavigateImgViewerImageFile(HWND hwnd, ImgViewerContext* context, int direction);
void HandleImgViewerOpenImageCommand(HWND hwnd, ImgViewerContext* context);
void HandleImgViewerCaptureRegion(HWND hwnd, ImgViewerContext* context);
void HandleImgViewerSaveImageAsCommand(HWND hwnd, ImgViewerContext* context);
void HandleImgViewerPasteClipboard(HWND hwnd, ImgViewerContext* context);
HRESULT OpenImgViewerSettingsWindow(HWND owner, ImgViewerContext* context);
HRESULT OpenImgViewerAboutWindow(HWND owner, ImgViewerContext* context);
HRESULT OpenImgViewerDeveloperWindow(HWND owner, ImgViewerContext* context);
void CleanupImgViewerOwnedWindow(ImgViewerContext* context, UiWindowDelegate* window);
