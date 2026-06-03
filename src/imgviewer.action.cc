#include "imgviewer.action.hpp"

#include <string_view>

const char* ImgViewerActionName(ImgViewerAction action)
{
    switch (action) {
    case ImgViewerAction::OpenImage:
        return "openImage";
    case ImgViewerAction::PreviousImage:
        return "previousImage";
    case ImgViewerAction::NextImage:
        return "nextImage";
    case ImgViewerAction::ZoomIn:
        return "zoomIn";
    case ImgViewerAction::ZoomOut:
        return "zoomOut";
    case ImgViewerAction::RotateClockwise:
        return "rotateClockwise";
    case ImgViewerAction::FlipHorizontal:
        return "flipHorizontal";
    case ImgViewerAction::FlipVertical:
        return "flipVertical";
    case ImgViewerAction::ResetView:
        return "resetView";
    case ImgViewerAction::OpenMenu:
        return "openMenu";
    case ImgViewerAction::OpenSettings:
        return "openSettings";
    case ImgViewerAction::CloseSettings:
        return "closeSettings";
    case ImgViewerAction::SaveSettings:
        return "saveSettings";
    case ImgViewerAction::ResetKeyBindings:
        return "resetKeyBindings";
    case ImgViewerAction::ToggleTopMost:
        return "toggleTopMost";
    case ImgViewerAction::Minimize:
        return "minimize";
    case ImgViewerAction::ToggleMaximize:
        return "toggleMaximize";
    case ImgViewerAction::Close:
        return "close";
    case ImgViewerAction::None:
    default:
        return "";
    }
}

ImgViewerAction ImgViewerActionFromName(const char* name)
{
    const std::string_view value = name != nullptr ? std::string_view(name) : std::string_view();
    if (value == "openImage") {
        return ImgViewerAction::OpenImage;
    }
    if (value == "previousImage") {
        return ImgViewerAction::PreviousImage;
    }
    if (value == "nextImage") {
        return ImgViewerAction::NextImage;
    }
    if (value == "zoomIn") {
        return ImgViewerAction::ZoomIn;
    }
    if (value == "zoomOut") {
        return ImgViewerAction::ZoomOut;
    }
    if (value == "rotateClockwise") {
        return ImgViewerAction::RotateClockwise;
    }
    if (value == "flipHorizontal") {
        return ImgViewerAction::FlipHorizontal;
    }
    if (value == "flipVertical") {
        return ImgViewerAction::FlipVertical;
    }
    if (value == "resetView") {
        return ImgViewerAction::ResetView;
    }
    if (value == "openMenu") {
        return ImgViewerAction::OpenMenu;
    }
    if (value == "openSettings") {
        return ImgViewerAction::OpenSettings;
    }
    if (value == "closeSettings") {
        return ImgViewerAction::CloseSettings;
    }
    if (value == "saveSettings") {
        return ImgViewerAction::SaveSettings;
    }
    if (value == "resetKeyBindings") {
        return ImgViewerAction::ResetKeyBindings;
    }
    if (value == "toggleTopMost") {
        return ImgViewerAction::ToggleTopMost;
    }
    if (value == "minimize") {
        return ImgViewerAction::Minimize;
    }
    if (value == "toggleMaximize") {
        return ImgViewerAction::ToggleMaximize;
    }
    if (value == "close") {
        return ImgViewerAction::Close;
    }
    return ImgViewerAction::None;
}
