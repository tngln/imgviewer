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
#include "imgviewer.viewer.hpp"
#include "ui.hpp"
#include "ui.renderer.hpp"

constexpr wchar_t kImgViewerWindowTitle[] = L"ImgViewer";

struct ImgViewerContext final {
    UiRenderer renderer;
    UiController ui;
    ImgViewerController viewer;
    ImageSequence sequence;
    ImgViewerConfig config;
    std::array<ImgViewerAction, 256> pressed_key_actions = {};
    wil::com_ptr<IRawElementProviderSimple> accessibility_provider;
    wil::unique_hwnd tooltip;
};

HRESULT RenderImgViewer(ImgViewerContext* context);
void SyncWindowState(HWND hwnd, UiController* ui);
void SaveWindowSize(HWND hwnd, ImgViewerContext* context);
bool IsImgViewerActionEnabled(const ImgViewerContext* context, ImgViewerAction action);
void SyncActionStates(ImgViewerContext* context);
void ExecuteImgViewerAction(HWND hwnd, ImgViewerContext* context, ImgViewerAction action);
void LoadImgViewerImageFile(HWND hwnd, ImgViewerContext* context, const wchar_t* path);
bool NavigateImgViewerImageFile(HWND hwnd, ImgViewerContext* context, int direction);
void HandleImgViewerOpenImageCommand(HWND hwnd, ImgViewerContext* context);
