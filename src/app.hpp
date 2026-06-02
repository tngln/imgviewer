#pragma once

#include <windows.h>

#include <ole2.h>
#include <UIAutomationCore.h>

#include <array>

#include <wil/com.h>
#include <wil/resource.h>

#include "app.action.hpp"
#include "app.config.hpp"
#include "image.sequence.hpp"
#include "image.viewer.hpp"
#include "ui.hpp"
#include "ui.renderer.hpp"

constexpr wchar_t kAppWindowTitle[] = L"ImgViewer";

struct AppContext final {
    UiRenderer renderer;
    UiController ui;
    ImageViewerController viewer;
    ImageSequence sequence;
    AppConfig config;
    std::array<AppAction, 256> pressed_key_actions = {};
    wil::com_ptr<IRawElementProviderSimple> accessibility_provider;
    wil::unique_hwnd tooltip;
};

HRESULT RenderApplication(AppContext* context);
void SyncWindowState(HWND hwnd, UiController* ui);
void SaveWindowSize(HWND hwnd, AppContext* context);
bool IsAppActionEnabled(const AppContext* context, AppAction action);
void SyncActionStates(AppContext* context);
void ExecuteAppAction(HWND hwnd, AppContext* context, AppAction action);
void LoadAppImageFile(HWND hwnd, AppContext* context, const wchar_t* path);
bool NavigateAppImageFile(HWND hwnd, AppContext* context, int direction);
void HandleOpenImageCommand(HWND hwnd, AppContext* context);
