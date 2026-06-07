#pragma once

#include <windows.h>

constexpr UINT kImgViewerOpenImageMessage = WM_APP + 1;
constexpr UINT kImgViewerUiActionMessage = WM_APP + 2;
constexpr UINT kImgViewerSettingsOpacityChangedMessage = WM_APP + 3;
constexpr UINT kImgViewerSettingsDestroyedMessage = WM_APP + 4;
constexpr UINT kImgViewerAboutDestroyedMessage = WM_APP + 5;
constexpr UINT_PTR kImgViewerToastTimerId = 1;
constexpr UINT_PTR kImgViewerAnimationTimerId = 2;
