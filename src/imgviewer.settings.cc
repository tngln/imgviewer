#include "imgviewer.settings.hpp"

#include <algorithm>
#include <array>
#include <cwchar>
#include <cwctype>
#include <memory>
#include <string>
#include <vector>

#include <d2d1helper.h>
#include <dwrite.h>
#include <imm.h>
#include <windowsx.h>
#include <wil/com.h>
#include <wil/result_macros.h>

#include "imgviewer.hpp"
#include "imgviewer.action.hpp"
#include "imgviewer.config.hpp"
#include "imgviewer.keybindings.hpp"
#include "imgviewer.messages.hpp"
#include "imgviewer.ui.action.hpp"
#include "ui.a11y.hpp"
#include "ui.button.hpp"
#include "ui.draw.hpp"
#include "ui.popup.hpp"
#include "ui.selection.hpp"
#include "ui.slider.hpp"
#include "ui.textbox.hpp"
#include "ui.theme.hpp"
#include "ui.window.hpp"

namespace {

constexpr wchar_t kSettingsClassName[] = L"ImgViewerSettingsWindow";

constexpr std::array<ImgViewerAction, 11> kShownActions{
    ImgViewerAction::OpenImage,
    ImgViewerAction::PreviousImage,
    ImgViewerAction::NextImage,
    ImgViewerAction::ZoomIn,
    ImgViewerAction::ZoomOut,
    ImgViewerAction::FitWindow,
    ImgViewerAction::ActualSize,
    ImgViewerAction::RotateClockwise,
    ImgViewerAction::FlipHorizontal,
    ImgViewerAction::FlipVertical,
    ImgViewerAction::ResetView,
};

constexpr wchar_t kSaveIcon[] = L"\xE105";
constexpr wchar_t kCancelIcon[] = L"\xE711";
constexpr wchar_t kResetIcon[] = L"\xE777";
constexpr UINT_PTR kCaretTimerId = 1;
constexpr int kOpacityMinimum = 10;
constexpr int kOpacityMaximum = 100;
constexpr int kOpacitySmallStep = 1;
constexpr int kOpacityLargeStep = 5;

const wchar_t* ActionDisplayName(ImgViewerAction action)
{
    switch (action) {
    case ImgViewerAction::OpenImage:
        return L"Open Image";
    case ImgViewerAction::PreviousImage:
        return L"Previous Image";
    case ImgViewerAction::NextImage:
        return L"Next Image";
    case ImgViewerAction::ZoomIn:
        return L"Zoom In";
    case ImgViewerAction::ZoomOut:
        return L"Zoom Out";
    case ImgViewerAction::FitWindow:
        return L"Fit Window";
    case ImgViewerAction::ActualSize:
        return L"Actual Size";
    case ImgViewerAction::RotateClockwise:
        return L"Rotate Clockwise";
    case ImgViewerAction::FlipHorizontal:
        return L"Flip Horizontal";
    case ImgViewerAction::FlipVertical:
        return L"Flip Vertical";
    case ImgViewerAction::ResetView:
        return L"Reset View";
    default:
        return L"";
    }
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

std::wstring ShortcutsForAction(const ActionBindings& bindings, ImgViewerAction action)
{
    std::wstring text;
    for (const KeyBinding& binding : bindings.key_bindings) {
        if (binding.action != action) {
            continue;
        }
        if (!text.empty()) {
            text += L", ";
        }
        text += GestureText(binding.gesture);
    }
    return text.empty() ? L"No shortcut configured." : text;
}

UiElementMetadata Metadata(
    UiElementId id,
    UiElementRole role,
    ImgViewerAction action,
    const wchar_t* name,
    const wchar_t* automation_id)
{
    return UiElementMetadata{
        .id = id,
        .role = role,
        .action = action,
        .name = name,
        .tooltip = name,
        .automation_id = automation_id,
    };
}

class SettingsUi final : public UiRoot {
public:
    explicit SettingsUi(ImgViewerConfig config) : draft_(std::move(config))
    {
        root_ = std::make_unique<UiElement>(
            Metadata(UiElementId::None, UiElementRole::Pane, ImgViewerAction::None, L"Settings", L"settings-root"));

        remember_checkbox_ = static_cast<Checkbox*>(root_->AddChild(std::make_unique<Checkbox>(
            Metadata(ids_.Next(), UiElementRole::CheckBox, ImgViewerAction::None, L"Remember window size", L"remember-window-size"),
            L"Remember window size",
            draft_.remember_window_size)));
        remember_radio_ = static_cast<RadioButton*>(root_->AddChild(std::make_unique<RadioButton>(
            Metadata(ids_.Next(), UiElementRole::RadioButton, ImgViewerAction::None, L"Remember last size", L"remember-last-size"),
            L"Remember last size",
            draft_.remember_window_size)));
        default_radio_ = static_cast<RadioButton*>(root_->AddChild(std::make_unique<RadioButton>(
            Metadata(ids_.Next(), UiElementRole::RadioButton, ImgViewerAction::None, L"Use default size", L"use-default-size"),
            L"Use default size",
            !draft_.remember_window_size)));
        pixelated_checkbox_ = static_cast<Checkbox*>(root_->AddChild(std::make_unique<Checkbox>(
            Metadata(ids_.Next(), UiElementRole::CheckBox, ImgViewerAction::None, L"Pixelated sampling", L"pixelated-sampling"),
            L"Pixelated sampling",
            draft_.pixelated_sampling)));
        borderless_checkbox_ = static_cast<Checkbox*>(root_->AddChild(std::make_unique<Checkbox>(
            Metadata(ids_.Next(), UiElementRole::CheckBox, ImgViewerAction::None, L"Borderless window", L"borderless-window"),
            L"Borderless window",
            draft_.borderless_window)));
        opacity_slider_ = static_cast<Slider*>(root_->AddChild(std::make_unique<Slider>(
            Metadata(ids_.Next(), UiElementRole::Slider, ImgViewerAction::None, L"Opacity", L"window-opacity"),
            kOpacityMinimum,
            kOpacityMaximum,
            draft_.window_opacity_percent,
            kOpacitySmallStep,
            kOpacityLargeStep)));

        filter_box_ = static_cast<TextBox*>(root_->AddChild(std::make_unique<TextBox>(
            Metadata(ids_.Next(), UiElementRole::Edit, ImgViewerAction::None, L"Shortcut filter", L"shortcut-filter"),
            L"Filter actions")));
        action_dropdown_ = static_cast<Dropdown*>(root_->AddChild(std::make_unique<Dropdown>(
            Metadata(ids_.Next(), UiElementRole::ComboBox, ImgViewerAction::None, L"Action shortcuts", L"action-shortcuts"),
            BuildDropdownOptions())));

        reset_button_ = static_cast<Button*>(root_->AddChild(std::make_unique<Button>(
            Metadata(ids_.Next(), UiElementRole::Button, ImgViewerAction::ResetKeyBindings, L"Reset Shortcuts", L"reset-shortcuts"),
            kResetIcon,
            L"Reset")));
        save_button_ = static_cast<Button*>(root_->AddChild(std::make_unique<Button>(
            Metadata(ids_.Next(), UiElementRole::Button, ImgViewerAction::SaveSettings, L"Save", L"save-settings"),
            kSaveIcon,
            L"Save")));
        cancel_button_ = static_cast<Button*>(root_->AddChild(std::make_unique<Button>(
            Metadata(ids_.Next(), UiElementRole::Button, ImgViewerAction::CloseSettings, L"Cancel", L"cancel-settings"),
            kCancelIcon,
            L"Cancel")));

        SyncChoiceControls();
        UpdateOpacityText();
        UpdateFilterResults();
        UpdateShortcutText();
    }

    const ImgViewerConfig& Draft() const { return draft_; }
    int OpacityPercent() const { return draft_.window_opacity_percent; }

    void SetOpacityPercent(int percent)
    {
        draft_.window_opacity_percent = ClampWindowOpacityPercent(percent);
        opacity_slider_->SetValue(draft_.window_opacity_percent);
        UpdateOpacityText();
    }

    UiElement* Root() override { return root_.get(); }
    const UiElement* Root() const override { return root_.get(); }
    const wchar_t* AccessibilityRootName() const override { return L"Settings"; }

    const wchar_t* ElementValue(UiElementId id) const override
    {
        if (id == filter_box_->Id()) {
            return filter_box_->Text().c_str();
        }
        if (id == opacity_slider_->Id()) {
            return opacity_text_.c_str();
        }
        return L"";
    }

    double ElementRangeValue(UiElementId id) const override
    {
        return id == opacity_slider_->Id() ? static_cast<double>(opacity_slider_->Value()) : 0.0;
    }

    double ElementRangeMinimum(UiElementId id) const override
    {
        return id == opacity_slider_->Id() ? static_cast<double>(opacity_slider_->Minimum()) : 0.0;
    }

    double ElementRangeMaximum(UiElementId id) const override
    {
        return id == opacity_slider_->Id() ? static_cast<double>(opacity_slider_->Maximum()) : 0.0;
    }

    double ElementRangeSmallChange(UiElementId id) const override
    {
        return id == opacity_slider_->Id() ? static_cast<double>(kOpacitySmallStep) : 1.0;
    }

    double ElementRangeLargeChange(UiElementId id) const override
    {
        return id == opacity_slider_->Id() ? static_cast<double>(kOpacityLargeStep) : 10.0;
    }

    HRESULT SetElementRangeValue(UiElementId id, double value) override
    {
        if (id != opacity_slider_->Id()) {
            return E_NOTIMPL;
        }
        SetOpacityPercent(static_cast<int>(value + 0.5));
        return S_OK;
    }

    void Draw(const UiDrawContext& context, UiRootState state) override
    {
        const D2D1_SIZE_F size = context.viewport_size;
        Layout(size);
        const UiDraw draw(context);
        draw.Clear(ui_theme::color::kWindowBackground);
        draw.DrawBodyText(L"Settings", 8, D2D1::RectF(24.0f, 18.0f, size.width - 24.0f, 46.0f), ui_theme::color::kBodyText);
        draw.DrawBodyText(L"Window size", 11, D2D1::RectF(24.0f, 88.0f, size.width - 24.0f, 112.0f), ui_theme::color::kMutedText);
        draw.DrawBodyText(L"Image rendering", 15, D2D1::RectF(24.0f, 194.0f, size.width - 24.0f, 218.0f), ui_theme::color::kMutedText);
        draw.DrawBodyText(L"Window frame", 12, D2D1::RectF(24.0f, 272.0f, size.width - 24.0f, 296.0f), ui_theme::color::kMutedText);
        draw.DrawBodyText(L"Opacity", 7, D2D1::RectF(24.0f, 350.0f, size.width - 24.0f, 374.0f), ui_theme::color::kMutedText);
        draw.DrawBodyText(
            opacity_text_.c_str(),
            static_cast<UINT32>(opacity_text_.size()),
            D2D1::RectF(size.width - 88.0f, 376.0f, size.width - 24.0f, 404.0f),
            ui_theme::color::kBodyText,
            D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
        draw.DrawBodyText(L"Shortcut filter", 15, D2D1::RectF(24.0f, 438.0f, size.width - 24.0f, 462.0f), ui_theme::color::kMutedText);
        draw.DrawBodyText(L"Action shortcuts", 16, D2D1::RectF(24.0f, 516.0f, size.width - 24.0f, 540.0f), ui_theme::color::kMutedText);
        draw.DrawBodyText(
            shortcut_text_.c_str(),
            static_cast<UINT32>(shortcut_text_.size()),
            D2D1::RectF(24.0f, 594.0f, size.width - 24.0f, 620.0f),
            ui_theme::color::kBodyText,
            D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);

        DrawElement(*remember_checkbox_, context, state);
        DrawElement(*remember_radio_, context, state);
        DrawElement(*default_radio_, context, state);
        DrawElement(*pixelated_checkbox_, context, state);
        DrawElement(*borderless_checkbox_, context, state);
        DrawElement(*opacity_slider_, context, state);
        DrawElement(*reset_button_, context, state);
        DrawElement(*save_button_, context, state);
        DrawElement(*cancel_button_, context, state);
        DrawElement(*filter_box_, context, state);
        DrawElement(*action_dropdown_, context, state);
    }

    UiEventResult OnInputEvent(const UiInputEvent& event) override
    {
        switch (event.type) {
        case UiEventType::TextChar:
            return OnTextChar(event.focused, event.character);
        case UiEventType::ImeStartComposition:
            return event.focused == filter_box_->Id() ? UiEventResult{.handled = true, .wants_ime_position = true} : UiEventResult{};
        case UiEventType::ImeComposition:
            return event.focused == filter_box_->Id() ? filter_box_->OnInputEvent(event) : UiEventResult{};
        case UiEventType::ImeEndComposition:
            return event.focused == filter_box_->Id() ? filter_box_->OnInputEvent(event) : UiEventResult{};
        case UiEventType::Timer:
            if (event.focused == filter_box_->Id()) {
                return filter_box_->OnInputEvent(event);
            }
            return {};
        case UiEventType::OwnerDeactivated:
            action_dropdown_->Collapse();
            return UiEventResult{.handled = true, .needs_render = true};
        default:
            return {};
        }
    }

    UiEventResult OnKeyEvent(const UiKeyEvent& event) override
    {
        if (event.type != UiEventType::KeyDown) {
            return {};
        }
        const UINT virtual_key = event.virtual_key;
        if (virtual_key == VK_ESCAPE) {
            if (action_dropdown_->IsExpanded()) {
                action_dropdown_->Collapse();
                return UiEventResult{.handled = true, .needs_render = true};
            }
            return UiEventResult{.handled = true, .action = ImgViewerAction::CloseSettings};
        }
        return {};
    }

    UiEventResult OnTextChar(UiElementId focused, wchar_t ch)
    {
        if (focused != filter_box_->Id()) {
            return {};
        }
        UiEventResult result = filter_box_->OnInputEvent(UiInputEvent{.type = UiEventType::TextChar, .character = ch});
        if (result.handled) {
            UpdateFilterResults();
            UpdateShortcutText();
        }
        return result;
    }

    UiEventResult ExecuteTextAction(UiAction action, HWND hwnd)
    {
        UiEventResult result = filter_box_->ExecuteEditAction(action, hwnd);
        if (result.handled) {
            UpdateFilterResults();
            UpdateShortcutText();
        }
        return result;
    }

private:
    void Layout(D2D1_SIZE_F size)
    {
        root_->SetRect(D2D1::RectF(0.0f, 0.0f, size.width, size.height));
        remember_checkbox_->SetRect(D2D1::RectF(24.0f, 54.0f, size.width - 24.0f, 84.0f));
        remember_radio_->SetRect(D2D1::RectF(44.0f, 116.0f, size.width - 24.0f, 146.0f));
        default_radio_->SetRect(D2D1::RectF(44.0f, 146.0f, size.width - 24.0f, 176.0f));
        pixelated_checkbox_->SetRect(D2D1::RectF(24.0f, 224.0f, size.width - 24.0f, 254.0f));
        borderless_checkbox_->SetRect(D2D1::RectF(24.0f, 302.0f, size.width - 24.0f, 332.0f));
        opacity_slider_->SetRect(D2D1::RectF(24.0f, 380.0f, size.width - 104.0f, 410.0f));
        filter_box_->SetRect(D2D1::RectF(24.0f, 468.0f, size.width - 24.0f, 502.0f));
        action_dropdown_->SetRect(D2D1::RectF(24.0f, 546.0f, size.width - 24.0f, 580.0f));
        reset_button_->SetRect(D2D1::RectF(24.0f, size.height - 58.0f, 150.0f, size.height - 20.0f));
        cancel_button_->SetRect(D2D1::RectF(size.width - 132.0f, size.height - 58.0f, size.width - 12.0f, size.height - 20.0f));
        save_button_->SetRect(D2D1::RectF(size.width - 254.0f, size.height - 58.0f, size.width - 142.0f, size.height - 20.0f));
    }

    void ApplyElementEffect(UiElementId id) override
    {
        if (id == remember_checkbox_->Id()) {
            draft_.remember_window_size = !draft_.remember_window_size;
            SyncChoiceControls();
        } else if (id == pixelated_checkbox_->Id()) {
            draft_.pixelated_sampling = !draft_.pixelated_sampling;
            SyncChoiceControls();
        } else if (id == borderless_checkbox_->Id()) {
            draft_.borderless_window = !draft_.borderless_window;
            SyncChoiceControls();
        } else if (id == remember_radio_->Id()) {
            draft_.remember_window_size = true;
            SyncChoiceControls();
        } else if (id == default_radio_->Id()) {
            draft_.remember_window_size = false;
            SyncChoiceControls();
        } else if (id == opacity_slider_->Id()) {
            draft_.window_opacity_percent = ClampWindowOpacityPercent(opacity_slider_->Value());
            UpdateOpacityText();
        } else if (id == action_dropdown_->Id()) {
            UpdateShortcutText();
        }
    }

    void SyncChoiceControls()
    {
        remember_checkbox_->SetChecked(draft_.remember_window_size);
        remember_radio_->SetSelected(draft_.remember_window_size);
        default_radio_->SetSelected(!draft_.remember_window_size);
        pixelated_checkbox_->SetChecked(draft_.pixelated_sampling);
        borderless_checkbox_->SetChecked(draft_.borderless_window);
    }

    void UpdateShortcutText()
    {
        const ImgViewerAction action = ImgViewerActionFromUiAction(action_dropdown_->SelectedAction());
        shortcut_text_ = action != ImgViewerAction::None
            ? ShortcutsForAction(draft_.action_bindings, action)
            : std::wstring();
    }

    bool MatchesFilter(ImgViewerAction action) const
    {
        const std::wstring& filter = filter_box_->Text();
        if (filter.empty()) {
            return true;
        }
        std::wstring haystack = ActionDisplayName(action);
        haystack += L" ";
        haystack += ShortcutsForAction(draft_.action_bindings, action);
        std::wstring needle = filter;
        for (wchar_t& ch : haystack) {
            ch = static_cast<wchar_t>(towlower(ch));
        }
        for (wchar_t& ch : needle) {
            ch = static_cast<wchar_t>(towlower(ch));
        }
        return haystack.find(needle) != std::wstring::npos;
    }

    std::vector<DropdownOption> BuildDropdownOptions() const
    {
        std::vector<DropdownOption> options;
        for (ImgViewerAction action : kShownActions) {
            if (MatchesFilter(action)) {
                options.push_back(DropdownOption{ActionDisplayName(action), action});
            }
        }
        if (options.empty()) {
            options.push_back(DropdownOption{L"No matches", ImgViewerAction::None});
        }
        return options;
    }

    void UpdateFilterResults()
    {
        action_dropdown_->SetOptions(BuildDropdownOptions());
    }

    void UpdateOpacityText()
    {
        wchar_t text[16] = {};
        swprintf_s(text, L"%d%%", draft_.window_opacity_percent);
        opacity_text_ = text;
    }

    void DrawElement(UiElement& element, const UiDrawContext& context, UiRootState state) const
    {
        element.Draw(
            context,
            UiElementState{
                .hovered = state.hovered == element.Id(),
                .pressed = state.pressed == element.Id(),
                .active = state.focused == element.Id(),
                .enabled = element.IsEnabled(),
                .checked = IsCheckedElement(element.Id()),
                .expanded = element.Id() == action_dropdown_->Id() && action_dropdown_->IsExpanded(),
            });
    }

    bool IsCheckedElement(UiElementId id) const
    {
        return (id == remember_checkbox_->Id() && remember_checkbox_->IsChecked()) ||
            (id == pixelated_checkbox_->Id() && pixelated_checkbox_->IsChecked()) ||
            (id == borderless_checkbox_->Id() && borderless_checkbox_->IsChecked()) ||
            (id == remember_radio_->Id() && remember_radio_->IsSelected()) ||
            (id == default_radio_->Id() && default_radio_->IsSelected());
    }

    ImgViewerConfig draft_;
    UiElementIdGenerator ids_;
    std::unique_ptr<UiElement> root_;
    Checkbox* remember_checkbox_ = nullptr;
    Checkbox* pixelated_checkbox_ = nullptr;
    Checkbox* borderless_checkbox_ = nullptr;
    RadioButton* remember_radio_ = nullptr;
    RadioButton* default_radio_ = nullptr;
    Slider* opacity_slider_ = nullptr;
    Dropdown* action_dropdown_ = nullptr;
    TextBox* filter_box_ = nullptr;
    Button* reset_button_ = nullptr;
    Button* save_button_ = nullptr;
    Button* cancel_button_ = nullptr;
    std::wstring opacity_text_;
    std::wstring shortcut_text_;
};

struct SettingsWindowContext final : public UiWindowDelegate {
    HWND owner = nullptr;
    ImgViewerContext* app = nullptr;
    UiWindowHost host;
    SettingsUi* ui = nullptr;
    int original_opacity_percent = 100;
    bool saved = false;

    explicit SettingsWindowContext(int original_opacity) :
        original_opacity_percent(ClampWindowOpacityPercent(original_opacity))
    {
    }

    HRESULT OnCreate(UiWindowHost& window_host) override
    {
        if (app != nullptr) {
            app->settings_window = window_host.Hwnd();
        }
        return S_OK;
    }

    void OnDestroy(UiWindowHost&) override
    {
        if (app != nullptr) {
            if (!saved) {
                app->current_window_opacity_percent = original_opacity_percent;
                ApplyWindowOpacity(owner, original_opacity_percent);
            }
            app->settings_window = nullptr;
            PostMessageW(owner, kImgViewerSettingsDestroyedMessage, 0, reinterpret_cast<LPARAM>(this));
        }
    }

    bool OnUiAction(UiWindowHost& window_host, UiAction action) override;
    void OnUiValueChanged(UiWindowHost&, UiEventResult result) override;
    win32::WindowMessageResult OnUnhandledMessage(
        UiWindowHost& window_host,
        UINT message,
        WPARAM wparam,
        LPARAM lparam) override;
};

void CaptureCurrentWindowSize(HWND hwnd, ImgViewerConfig* config)
{
    if (hwnd == nullptr || config == nullptr || !config->remember_window_size || IsIconic(hwnd) || IsZoomed(hwnd)) {
        return;
    }

    RECT window_rect = {};
    if (!GetWindowRect(hwnd, &window_rect)) {
        return;
    }

    config->window_size.width = static_cast<int>(window_rect.right - window_rect.left);
    config->window_size.height = static_cast<int>(window_rect.bottom - window_rect.top);
}

void SaveSettings(HWND hwnd, SettingsWindowContext* context)
{
    if (context == nullptr) {
        DestroyWindow(hwnd);
        return;
    }

    if (context->app != nullptr && context->ui != nullptr) {
        ImgViewerConfig draft = context->ui->Draft();
        CaptureCurrentWindowSize(context->owner, &draft);
        const bool frame_changed = context->app->config.borderless_window != draft.borderless_window;
        context->app->config = draft;
        context->app->current_window_opacity_percent = context->app->config.window_opacity_percent;
        ApplyWindowOpacity(context->owner, context->app->current_window_opacity_percent);
        context->app->viewer.SetPixelatedSampling(context->app->config.pixelated_sampling);
        SaveImgViewerConfig(context->app->config);
        if (frame_changed) {
            ApplyImgViewerWindowFrame(context->owner, context->app, true);
        } else {
            RenderImgViewer(context->app);
        }
    }
    context->saved = true;
    context->host.Close();
}

bool SettingsWindowContext::OnUiAction(UiWindowHost& window_host, UiAction action)
{
    if (action == kUiActionTextCopy ||
        action == kUiActionTextCut ||
        action == kUiActionTextPaste ||
        action == kUiActionTextSelectAll) {
        if (ui != nullptr) {
            const UiEventResult result = ui->ExecuteTextAction(action, window_host.Hwnd());
            if (result.needs_render) {
                window_host.Invalidate();
            }
        }
        return true;
    }

    switch (ImgViewerActionFromUiAction(action)) {
    case ImgViewerAction::SaveSettings:
        SaveSettings(window_host.Hwnd(), this);
        return true;
    case ImgViewerAction::CloseSettings:
        window_host.Close();
        return true;
    case ImgViewerAction::ResetKeyBindings:
        if (ui != nullptr) {
            ImgViewerConfig draft = ui->Draft();
            draft.action_bindings = DefaultActionBindings();
            auto root = std::make_unique<SettingsUi>(std::move(draft));
            ui = root.get();
            window_host.ResetRoot(std::move(root));
        }
        return true;
    default:
        break;
    }

    return false;
}

void SettingsWindowContext::OnUiValueChanged(UiWindowHost&, UiEventResult)
{
    if (owner != nullptr && ui != nullptr) {
        if (app != nullptr) {
            app->current_window_opacity_percent = ui->OpacityPercent();
        }
        ApplyWindowOpacity(owner, ui->OpacityPercent());
    }
}

win32::WindowMessageResult SettingsWindowContext::OnUnhandledMessage(
    UiWindowHost& window_host,
    UINT message,
    WPARAM wparam,
    LPARAM)
{
    switch (message) {
    case kImgViewerSettingsOpacityChangedMessage: {
        if (ui != nullptr) {
            ui->SetOpacityPercent(static_cast<int>(wparam));
            window_host.Invalidate();
        }
        return win32::WindowMessageResult::Handled();
    }
    default:
        break;
    }
    return win32::WindowMessageResult::Unhandled();
}

} // namespace

HRESULT OpenImgViewerSettingsWindow(HWND owner, ImgViewerContext* context)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, owner);
    RETURN_HR_IF_NULL(E_INVALIDARG, context);
    if (context->settings_window != nullptr && IsWindow(context->settings_window)) {
        ShowWindow(context->settings_window, SW_SHOWNORMAL);
        SetForegroundWindow(context->settings_window);
        return S_OK;
    }

    HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner, GWLP_HINSTANCE));
    RETURN_HR_IF_NULL(E_UNEXPECTED, instance);

    ImgViewerConfig draft = context->config;
    draft.window_opacity_percent = context->current_window_opacity_percent;
    auto* settings_context = new (std::nothrow) SettingsWindowContext(context->current_window_opacity_percent);
    RETURN_IF_NULL_ALLOC(settings_context);
    settings_context->owner = owner;
    settings_context->app = context;
    context->settings_context = settings_context;

    auto root = std::make_unique<SettingsUi>(std::move(draft));
    settings_context->ui = root.get();
    const HRESULT create_hr = settings_context->host.Create(
        UiWindowOptions{
            .native = win32::NativeWindowOptions{
                .instance = instance,
                .class_name = kSettingsClassName,
                .title = L"Settings",
                .style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                .ex_style = WS_EX_DLGMODALFRAME,
                .width = 720,
                .height = 720,
                .owner = owner,
                .show_command = SW_SHOWNORMAL,
            },
            .action_message = kImgViewerUiActionMessage,
            .caret_timer_id = kCaretTimerId,
        },
        std::move(root),
        settings_context);
    if (FAILED(create_hr)) {
        context->settings_context = nullptr;
        delete settings_context;
        RETURN_IF_FAILED(create_hr);
    }

    settings_context->host.Window().Show(SW_SHOWNORMAL);
    return S_OK;
}

void CleanupImgViewerSettingsWindow(ImgViewerContext* context, void* settings_context)
{
    if (context != nullptr && context->settings_context == settings_context) {
        context->settings_context = nullptr;
    }
    delete static_cast<SettingsWindowContext*>(settings_context);
}
