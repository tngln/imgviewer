#pragma once

#include <windows.h>

#include <ole2.h>
#include <UIAutomationCore.h>

#include <array>
#include <wil/com.h>
#include <wil/resource.h>

#include "imgviewer.action.hpp"
#include "imgviewer.config.hpp"
#include "image.sequence.hpp"
#include "imgviewer.renderer.hpp"
#include "imgviewer.viewer.hpp"
#include "ui.hpp"

constexpr wchar_t kImgViewerWindowTitle[] = L"ImgViewer";

struct ImgViewerContext final {
    ImgViewerContext();

    ImgViewerRenderer renderer;
    UiController ui;
    ImgViewerController viewer;
    ImageSequence sequence;
    ImgViewerConfig config;
    int current_window_opacity_percent = 100;
    std::array<ImgViewerAction, 256> pressed_key_actions = {};
    wil::com_ptr<IRawElementProviderSimple> accessibility_provider;
    wil::unique_hwnd tooltip;
    HWND settings_window = nullptr;
    bool color_picker_active = false;
};

HRESULT RenderImgViewer(ImgViewerContext* context);
DWORD ImgViewerWindowStyle(bool borderless);
HRESULT ApplyImgViewerWindowFrame(HWND hwnd, ImgViewerContext* context, bool hide_for_transition);
void SyncWindowState(HWND hwnd, UiController* ui);
void SaveWindowSize(HWND hwnd, ImgViewerContext* context);
bool IsImgViewerActionEnabled(const ImgViewerContext* context, ImgViewerAction action);
void SyncActionStates(ImgViewerContext* context);
void ShowImgViewerToast(HWND hwnd, ImgViewerContext* context, const wchar_t* text);
void ApplyWindowOpacity(HWND hwnd, int percent);
void SetImgViewerWindowOpacity(HWND hwnd, ImgViewerContext* context, int percent);
void ExecuteImgViewerAction(HWND hwnd, ImgViewerContext* context, ImgViewerAction action);
bool HandleImgViewerColorPick(HWND hwnd, ImgViewerContext* context, D2D1_POINT_2F point);
void LoadImgViewerImageFile(HWND hwnd, ImgViewerContext* context, const wchar_t* path);
bool NavigateImgViewerImageFile(HWND hwnd, ImgViewerContext* context, int direction);
void HandleImgViewerOpenImageCommand(HWND hwnd, ImgViewerContext* context);
HRESULT OpenImgViewerSettingsWindow(HWND owner, ImgViewerContext* context);
