#include "imgviewer.action.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace {

struct ActionEntry {
    std::string_view name;
    ImgViewerAction action;
};

constexpr std::array kActionTable = std::array<ActionEntry, 30>{
    {{"actualSize", ImgViewerAction::ActualSize},
     {"close", ImgViewerAction::Close},
     {"closeAbout", ImgViewerAction::CloseAbout},
     {"closeSettings", ImgViewerAction::CloseSettings},
     {"copyAboutNotices", ImgViewerAction::CopyAboutNotices},
     {"fitWindow", ImgViewerAction::FitWindow},
     {"flipHorizontal", ImgViewerAction::FlipHorizontal},
     {"flipVertical", ImgViewerAction::FlipVertical},
     {"minimize", ImgViewerAction::Minimize},
     {"nextImage", ImgViewerAction::NextImage},
     {"openAbout", ImgViewerAction::OpenAbout},
     {"openImage", ImgViewerAction::OpenImage},
     {"openMenu", ImgViewerAction::OpenMenu},
     {"openSettings", ImgViewerAction::OpenSettings},
     {"previousImage", ImgViewerAction::PreviousImage},
     {"resetKeyBindings", ImgViewerAction::ResetKeyBindings},
     {"resetView", ImgViewerAction::ResetView},
     {"rotateClockwise", ImgViewerAction::RotateClockwise},
     {"saveImageAs", ImgViewerAction::SaveImageAs},
     {"saveSettings", ImgViewerAction::SaveSettings},
     {"textCopy", ImgViewerAction::TextCopy},
     {"textCut", ImgViewerAction::TextCut},
     {"textPaste", ImgViewerAction::TextPaste},
     {"textSelectAll", ImgViewerAction::TextSelectAll},
     {"toggleColorPicker", ImgViewerAction::ToggleColorPicker},
     {"toggleInfoPanel", ImgViewerAction::ToggleInfoPanel},
     {"toggleMaximize", ImgViewerAction::ToggleMaximize},
     {"toggleTopMost", ImgViewerAction::ToggleTopMost},
     {"zoomIn", ImgViewerAction::ZoomIn},
     {"zoomOut", ImgViewerAction::ZoomOut}},
};
} // namespace

const char* ImgViewerActionName(ImgViewerAction action)
{
    switch (action) {
    case ImgViewerAction::OpenImage:
        return "openImage";
    case ImgViewerAction::SaveImageAs:
        return "saveImageAs";
    case ImgViewerAction::PreviousImage:
        return "previousImage";
    case ImgViewerAction::NextImage:
        return "nextImage";
    case ImgViewerAction::ZoomIn:
        return "zoomIn";
    case ImgViewerAction::ZoomOut:
        return "zoomOut";
    case ImgViewerAction::FitWindow:
        return "fitWindow";
    case ImgViewerAction::ActualSize:
        return "actualSize";
    case ImgViewerAction::RotateClockwise:
        return "rotateClockwise";
    case ImgViewerAction::FlipHorizontal:
        return "flipHorizontal";
    case ImgViewerAction::FlipVertical:
        return "flipVertical";
    case ImgViewerAction::ResetView:
        return "resetView";
    case ImgViewerAction::ToggleColorPicker:
        return "toggleColorPicker";
    case ImgViewerAction::ToggleInfoPanel:
        return "toggleInfoPanel";
    case ImgViewerAction::OpenMenu:
        return "openMenu";
    case ImgViewerAction::OpenSettings:
        return "openSettings";
    case ImgViewerAction::CloseSettings:
        return "closeSettings";
    case ImgViewerAction::SaveSettings:
        return "saveSettings";
    case ImgViewerAction::OpenAbout:
        return "openAbout";
    case ImgViewerAction::CloseAbout:
        return "closeAbout";
    case ImgViewerAction::CopyAboutNotices:
        return "copyAboutNotices";
    case ImgViewerAction::ResetKeyBindings:
        return "resetKeyBindings";
    case ImgViewerAction::TextCopy:
        return "textCopy";
    case ImgViewerAction::TextCut:
        return "textCut";
    case ImgViewerAction::TextPaste:
        return "textPaste";
    case ImgViewerAction::TextSelectAll:
        return "textSelectAll";
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
    if (name == nullptr) {
        return ImgViewerAction::None;
    }

    const std::string_view value(name);
    for (const ActionEntry& entry : kActionTable) {
        if (entry.name == value) {
            return entry.action;
        }
    }
    return ImgViewerAction::None;
}
