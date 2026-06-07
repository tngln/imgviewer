#include "imgviewer.keybindings.hpp"

#include <algorithm>
#include <string>
#include <string_view>

namespace {

std::string ToLowerAscii(std::string_view value)
{
    std::string lowered;
    lowered.reserve(value.size());
    for (char ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            lowered.push_back(static_cast<char>(ch - 'A' + 'a'));
        } else {
            lowered.push_back(ch);
        }
    }
    return lowered;
}

bool ParseVirtualKey(std::string_view token, UINT* virtual_key)
{
    if (virtual_key == nullptr || token.empty()) {
        return false;
    }

    if (token.size() == 1) {
        const char ch = token[0];
        if (ch >= 'a' && ch <= 'z') {
            *virtual_key = static_cast<UINT>(ch - 'a' + 'A');
            return true;
        }
        if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
            *virtual_key = static_cast<UINT>(ch);
            return true;
        }
        if (ch == '=') {
            *virtual_key = VK_OEM_PLUS;
            return true;
        }
        if (ch == '-') {
            *virtual_key = VK_OEM_MINUS;
            return true;
        }
    }

    const std::string key = ToLowerAscii(token);
    if (key == "left") {
        *virtual_key = VK_LEFT;
    } else if (key == "right") {
        *virtual_key = VK_RIGHT;
    } else if (key == "up") {
        *virtual_key = VK_UP;
    } else if (key == "down") {
        *virtual_key = VK_DOWN;
    } else if (key == "escape" || key == "esc") {
        *virtual_key = VK_ESCAPE;
    } else if (key == "enter") {
        *virtual_key = VK_RETURN;
    } else if (key == "space") {
        *virtual_key = VK_SPACE;
    } else if (key == "delete" || key == "del") {
        *virtual_key = VK_DELETE;
    } else if (key == "backspace") {
        *virtual_key = VK_BACK;
    } else if (key == "home") {
        *virtual_key = VK_HOME;
    } else if (key == "end") {
        *virtual_key = VK_END;
    } else if (key == "pageup") {
        *virtual_key = VK_PRIOR;
    } else if (key == "pagedown") {
        *virtual_key = VK_NEXT;
    } else {
        return false;
    }

    return true;
}

bool ParseKeyGesture(std::string_view text, KeyGesture* gesture)
{
    if (gesture == nullptr || text.empty()) {
        return false;
    }

    KeyGesture parsed;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t end = text.find('+', start);
        const std::string_view token = text.substr(start, end == std::string_view::npos ? text.size() - start : end - start);
        if (token.empty()) {
            return false;
        }

        const std::string lowered = ToLowerAscii(token);
        if (lowered == "ctrl" || lowered == "control") {
            parsed.ctrl = true;
        } else if (lowered == "shift") {
            parsed.shift = true;
        } else if (lowered == "alt") {
            parsed.alt = true;
        } else if (parsed.virtual_key == 0 && ParseVirtualKey(token, &parsed.virtual_key)) {
        } else {
            return false;
        }

        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }

    if (parsed.virtual_key == 0) {
        return false;
    }

    *gesture = parsed;
    return true;
}

void SetBinding(ActionBindings* bindings, KeyGesture gesture, ImgViewerAction action)
{
    if (bindings == nullptr || action == ImgViewerAction::None) {
        return;
    }

    for (KeyBinding& binding : bindings->key_bindings) {
        if (binding.gesture.virtual_key == gesture.virtual_key &&
            binding.gesture.ctrl == gesture.ctrl &&
            binding.gesture.shift == gesture.shift &&
            binding.gesture.alt == gesture.alt) {
            binding.action = action;
            return;
        }
    }

    bindings->key_bindings.push_back(KeyBinding{
        .gesture = gesture,
        .action = action,
    });
}

void RemoveBindingsForAction(ActionBindings* bindings, ImgViewerAction action)
{
    if (bindings == nullptr || action == ImgViewerAction::None) {
        return;
    }

    auto& key_bindings = bindings->key_bindings;
    key_bindings.erase(
        std::remove_if(
            key_bindings.begin(),
            key_bindings.end(),
            [action](const KeyBinding& binding) {
                return binding.action == action;
            }),
        key_bindings.end());
}

} // namespace

ActionBindings DefaultActionBindings()
{
    ActionBindings bindings;
    SetBinding(&bindings, KeyGesture{.virtual_key = VK_LEFT}, ImgViewerAction::PreviousImage);
    SetBinding(&bindings, KeyGesture{.virtual_key = VK_RIGHT}, ImgViewerAction::NextImage);
    SetBinding(&bindings, KeyGesture{.virtual_key = 'R'}, ImgViewerAction::RotateClockwise);
    SetBinding(&bindings, KeyGesture{.virtual_key = 'H'}, ImgViewerAction::FlipHorizontal);
    SetBinding(&bindings, KeyGesture{.virtual_key = 'V'}, ImgViewerAction::FlipVertical);
    SetBinding(&bindings, KeyGesture{.virtual_key = VK_OEM_PLUS, .ctrl = true}, ImgViewerAction::ZoomIn);
    SetBinding(&bindings, KeyGesture{.virtual_key = VK_OEM_MINUS, .ctrl = true}, ImgViewerAction::ZoomOut);
    SetBinding(&bindings, KeyGesture{.virtual_key = 'I'}, ImgViewerAction::ToggleInfoPanel);
    SetBinding(&bindings, KeyGesture{.virtual_key = '9', .ctrl = true}, ImgViewerAction::FitWindow);
    SetBinding(&bindings, KeyGesture{.virtual_key = '1', .ctrl = true}, ImgViewerAction::ActualSize);
    SetBinding(&bindings, KeyGesture{.virtual_key = '0', .ctrl = true}, ImgViewerAction::ResetView);
    SetBinding(&bindings, KeyGesture{.virtual_key = 'O', .ctrl = true}, ImgViewerAction::OpenImage);
    return bindings;
}

void ApplyKeyBindingsConfig(const nlohmann::json& root, ActionBindings* bindings)
{
    if (bindings == nullptr) {
        return;
    }

    const auto key_bindings = root.find("keyBindings");
    if (key_bindings == root.end() || !key_bindings->is_object()) {
        return;
    }

    for (const auto& item : key_bindings->items()) {
        const ImgViewerAction action = ImgViewerActionFromName(item.key().c_str());
        if (!IsImgViewerActionConfigurableKeyBinding(action) || !item.value().is_array()) {
            continue;
        }

        RemoveBindingsForAction(bindings, action);
        for (const nlohmann::json& key_value : item.value()) {
            if (!key_value.is_string()) {
                continue;
            }

            KeyGesture gesture;
            const std::string key_text = key_value.get<std::string>();
            if (ParseKeyGesture(key_text, &gesture)) {
                SetBinding(bindings, gesture, action);
            }
        }
    }
}

ImgViewerAction ActionForKey(const ActionBindings& bindings, UINT virtual_key, bool ctrl, bool shift, bool alt)
{
    for (const KeyBinding& binding : bindings.key_bindings) {
        if (binding.gesture.virtual_key == virtual_key &&
            binding.gesture.ctrl == ctrl &&
            binding.gesture.shift == shift &&
            binding.gesture.alt == alt) {
            return binding.action;
        }
    }

    return ImgViewerAction::None;
}

std::wstring KeyName(UINT virtual_key)
{
    if (virtual_key >= 'A' && virtual_key <= 'Z') {
        return std::wstring(1, static_cast<wchar_t>(virtual_key));
    }
    if (virtual_key >= '0' && virtual_key <= '9') {
        return std::wstring(1, static_cast<wchar_t>(virtual_key));
    }
    switch (virtual_key) {
    case VK_LEFT:
        return L"Left";
    case VK_RIGHT:
        return L"Right";
    case VK_OEM_PLUS:
        return L"=";
    case VK_OEM_MINUS:
        return L"-";
    default:
        return L"Key";
    }
}

std::wstring GestureText(const KeyGesture& gesture)
{
    std::wstring text;
    if (gesture.ctrl) {
        text += L"Ctrl+";
    }
    if (gesture.shift) {
        text += L"Shift+";
    }
    if (gesture.alt) {
        text += L"Alt+";
    }
    text += KeyName(gesture.virtual_key);
    return text;
}
