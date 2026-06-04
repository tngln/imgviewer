#pragma once

#include "imgviewer.action.hpp"
#include "ui.action.hpp"

constexpr UiAction UiActionFromImgViewerAction(ImgViewerAction action)
{
    return UiAction(action);
}

constexpr ImgViewerAction ImgViewerActionFromUiAction(UiAction action)
{
    return static_cast<ImgViewerAction>(UiActionValue(action));
}
