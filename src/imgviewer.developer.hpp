#pragma once

#include <windows.h>

struct ImgViewerContext;

HRESULT OpenImgViewerDeveloperWindow(HWND owner, ImgViewerContext* context);
void CleanupImgViewerDeveloperWindow(ImgViewerContext* context, void* developer_context);
HRESULT RunImgViewerDeveloperWindowApplication();
