#include "app.action.hpp"

#include <string_view>

const char* AppActionName(AppAction action)
{
    switch (action) {
    case AppAction::OpenImage:
        return "openImage";
    case AppAction::PreviousImage:
        return "previousImage";
    case AppAction::NextImage:
        return "nextImage";
    case AppAction::ZoomIn:
        return "zoomIn";
    case AppAction::ZoomOut:
        return "zoomOut";
    case AppAction::RotateClockwise:
        return "rotateClockwise";
    case AppAction::FlipHorizontal:
        return "flipHorizontal";
    case AppAction::FlipVertical:
        return "flipVertical";
    case AppAction::ResetView:
        return "resetView";
    case AppAction::ToggleTopMost:
        return "toggleTopMost";
    case AppAction::Minimize:
        return "minimize";
    case AppAction::ToggleMaximize:
        return "toggleMaximize";
    case AppAction::Close:
        return "close";
    case AppAction::None:
    default:
        return "";
    }
}

AppAction AppActionFromName(const char* name)
{
    const std::string_view value = name != nullptr ? std::string_view(name) : std::string_view();
    if (value == "openImage") {
        return AppAction::OpenImage;
    }
    if (value == "previousImage") {
        return AppAction::PreviousImage;
    }
    if (value == "nextImage") {
        return AppAction::NextImage;
    }
    if (value == "zoomIn") {
        return AppAction::ZoomIn;
    }
    if (value == "zoomOut") {
        return AppAction::ZoomOut;
    }
    if (value == "rotateClockwise") {
        return AppAction::RotateClockwise;
    }
    if (value == "flipHorizontal") {
        return AppAction::FlipHorizontal;
    }
    if (value == "flipVertical") {
        return AppAction::FlipVertical;
    }
    if (value == "resetView") {
        return AppAction::ResetView;
    }
    if (value == "toggleTopMost") {
        return AppAction::ToggleTopMost;
    }
    if (value == "minimize") {
        return AppAction::Minimize;
    }
    if (value == "toggleMaximize") {
        return AppAction::ToggleMaximize;
    }
    if (value == "close") {
        return AppAction::Close;
    }
    return AppAction::None;
}
