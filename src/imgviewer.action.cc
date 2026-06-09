#include "imgviewer.action.hpp"

#include <array>
#include <string_view>

namespace {

constexpr std::array kActionTable{
#define IMGVIEWER_ACTION_INFO(value, name, display_name, configurable_key, shown_in_settings) \
    ImgViewerActionInfo{ImgViewerAction::value, name, display_name, configurable_key, shown_in_settings},
    IMGVIEWER_ACTIONS(IMGVIEWER_ACTION_INFO)
#undef IMGVIEWER_ACTION_INFO
};
} // namespace

std::span<const ImgViewerActionInfo> ImgViewerActions()
{
    return kActionTable;
}

const ImgViewerActionInfo* ImgViewerActionInfoFor(ImgViewerAction action)
{
    for (const ImgViewerActionInfo& entry : kActionTable) {
        if (entry.action == action) {
            return &entry;
        }
    }
    return nullptr;
}

const char* ImgViewerActionName(ImgViewerAction action)
{
    const ImgViewerActionInfo* info = ImgViewerActionInfoFor(action);
    return info != nullptr ? info->name : "";
}

const wchar_t* ImgViewerActionDisplayName(ImgViewerAction action)
{
    const ImgViewerActionInfo* info = ImgViewerActionInfoFor(action);
    return info != nullptr ? ImgViewerString(info->display_name) : L"";
}

ImgViewerAction ImgViewerActionFromName(const char* name)
{
    if (name == nullptr) {
        return ImgViewerAction::None;
    }

    const std::string_view value(name);
    for (const ImgViewerActionInfo& entry : kActionTable) {
        if (entry.name == value) {
            return entry.action;
        }
    }
    return ImgViewerAction::None;
}

bool IsImgViewerActionConfigurableKeyBinding(ImgViewerAction action)
{
    const ImgViewerActionInfo* info = ImgViewerActionInfoFor(action);
    return info != nullptr && info->configurable_key;
}

bool IsImgViewerActionShownInSettings(ImgViewerAction action)
{
    const ImgViewerActionInfo* info = ImgViewerActionInfoFor(action);
    return info != nullptr && info->shown_in_settings;
}
