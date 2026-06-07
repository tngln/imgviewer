#include "imgviewer.settings.hpp"

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
#include "win32.util.hpp"
#include "ui.button.hpp"
#include "ui.draw.hpp"
#include "ui.label.hpp"
#include "ui.panel.hpp"
#include "ui.selection.hpp"
#include "ui.slider.hpp"
#include "ui.textbox.hpp"
#include "ui.theme.hpp"
#include "ui.window.hpp"

namespace {

constexpr wchar_t kSettingsClassName[] = L"ImgViewerSettingsWindow";

constexpr wchar_t kSaveIcon[] = L"\xE105";
constexpr wchar_t kCancelIcon[] = L"\xE711";
constexpr wchar_t kResetIcon[] = L"\xE777";
constexpr UINT_PTR kCaretTimerId = 1;
constexpr int kOpacityMinimum = 10;
constexpr int kOpacityMaximum = 100;
constexpr int kOpacitySmallStep = 1;
constexpr int kOpacityLargeStep = 5;
constexpr int kToolbarScaleMinimum = 80;
constexpr int kToolbarScaleMaximum = 160;
constexpr int kToolbarScaleSmallStep = 5;
constexpr int kToolbarScaleLargeStep = 10;
constexpr int kSettingsInitialWidth = 410;
constexpr int kSettingsInitialHeight = 472;
constexpr int kSettingsMinClientWidth = 310;
constexpr int kSettingsMinClientHeight = 458;

constexpr float kSettingsSidePadding = 14.0f;
constexpr float kSettingsContentTopPadding = 9.0f;
constexpr float kSettingsFooterBottomPadding = 10.0f;
constexpr float kSettingsFooterButtonHeight = 24.0f;
constexpr float kSettingsFooterButtonGap = 5.0f;

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


class SettingsUi final : public UiRoot {
public:
    explicit SettingsUi(ImgViewerConfig config) : draft_(std::move(config))
    {
        auto root_panel = std::make_unique<StackPanel>(
            UiRootMetadata(
                UiElementRole::Pane,
                UiActionFromImgViewerAction(ImgViewerAction::None),
                L"Settings",
                L"Settings",
                L"settings-root"));
        root_panel->SetPadding(UiThickness{kSettingsSidePadding, kSettingsContentTopPadding, kSettingsSidePadding, 0.0f});
        root_panel->SetGap(0.0f);
        root_ = root_panel.get();
        root_owner_ = std::move(root_panel);

        BuildUiTree();
        SyncChoiceControls();
        UpdateOpacityText();
        UpdateToolbarScaleText();
        UpdateFilterResults();
        UpdateShortcutText();
    }

    const ImgViewerConfig& Draft() const { return draft_; }
    int OpacityPercent() const { return draft_.window_opacity_percent; }
    int ToolbarScalePercent() const { return draft_.toolbar_scale_percent; }

    void SetOpacityPercent(int percent)
    {
        draft_.window_opacity_percent = ClampWindowOpacityPercent(percent);
        opacity_slider_row_->SetValue(draft_.window_opacity_percent);
        UpdateOpacityText();
        opacity_slider_row_->SetValueText(opacity_text_.c_str());
    }

    void SetToolbarScalePercent(int percent) override
    {
        draft_.toolbar_scale_percent = ClampToolbarScalePercent(percent);
        toolbar_scale_slider_row_->SetValue(draft_.toolbar_scale_percent);
        UpdateToolbarScaleText();
        toolbar_scale_slider_row_->SetValueText(toolbar_scale_text_.c_str());
    }

    UiElement* Root() override { return root_; }
    const UiElement* Root() const override { return root_; }
    const wchar_t* AccessibilityRootName() const override { return L"Settings"; }

    const wchar_t* ElementValue(UiElementId id) const override
    {
        if (id == filter_box_->Id()) {
            return filter_box_->Text().c_str();
        }
        if (id == opacity_slider_row_->GetSlider()->Id()) {
            return opacity_text_.c_str();
        }
        if (id == toolbar_scale_slider_row_->GetSlider()->Id()) {
            return toolbar_scale_text_.c_str();
        }
        return L"";
    }

    double ElementRangeValue(UiElementId id) const override
    {
        if (id == opacity_slider_row_->GetSlider()->Id()) {
            return static_cast<double>(opacity_slider_row_->Value());
        }
        if (id == toolbar_scale_slider_row_->GetSlider()->Id()) {
            return static_cast<double>(toolbar_scale_slider_row_->Value());
        }
        return 0.0;
    }

    double ElementRangeMinimum(UiElementId id) const override
    {
        if (id == opacity_slider_row_->GetSlider()->Id()) {
            return static_cast<double>(kOpacityMinimum);
        }
        if (id == toolbar_scale_slider_row_->GetSlider()->Id()) {
            return static_cast<double>(kToolbarScaleMinimum);
        }
        return 0.0;
    }

    double ElementRangeMaximum(UiElementId id) const override
    {
        if (id == opacity_slider_row_->GetSlider()->Id()) {
            return static_cast<double>(kOpacityMaximum);
        }
        if (id == toolbar_scale_slider_row_->GetSlider()->Id()) {
            return static_cast<double>(kToolbarScaleMaximum);
        }
        return 0.0;
    }

    double ElementRangeSmallChange(UiElementId id) const override
    {
        if (id == opacity_slider_row_->GetSlider()->Id()) {
            return static_cast<double>(kOpacitySmallStep);
        }
        if (id == toolbar_scale_slider_row_->GetSlider()->Id()) {
            return static_cast<double>(kToolbarScaleSmallStep);
        }
        return 1.0;
    }

    double ElementRangeLargeChange(UiElementId id) const override
    {
        if (id == opacity_slider_row_->GetSlider()->Id()) {
            return static_cast<double>(kOpacityLargeStep);
        }
        if (id == toolbar_scale_slider_row_->GetSlider()->Id()) {
            return static_cast<double>(kToolbarScaleLargeStep);
        }
        return 10.0;
    }

    HRESULT SetElementRangeValue(UiElementId id, double value) override
    {
        if (id == opacity_slider_row_->GetSlider()->Id()) {
            SetOpacityPercent(static_cast<int>(value + 0.5));
            return S_OK;
        }
        if (id == toolbar_scale_slider_row_->GetSlider()->Id()) {
            SetToolbarScalePercent(static_cast<int>(value + 0.5));
            return S_OK;
        }
        return E_NOTIMPL;
    }

    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) override
    {
        reset_button_width_ = reset_button_->PreferredWidth(context);
        save_button_width_ = save_button_->PreferredWidth(context);
        cancel_button_width_ = cancel_button_->PreferredWidth(context);
        const float footer_height = kSettingsFooterButtonHeight + kSettingsFooterBottomPadding;
        D2D1_SIZE_F content_available = D2D1::SizeF(available_size.width, available_size.height - footer_height);
        root_->Measure(context, content_available);
        return available_size;
    }

    void Arrange(D2D1_RECT_F final_rect) override
    {
        const float footer_top = final_rect.bottom - kSettingsFooterButtonHeight - kSettingsFooterBottomPadding;
        const float footer_bottom = footer_top + kSettingsFooterButtonHeight;

        // Footer buttons: reset left, save+cancel right
        reset_button_->Arrange(D2D1::RectF(
            final_rect.left + kSettingsSidePadding,
            footer_top,
            final_rect.left + kSettingsSidePadding + reset_button_width_,
            footer_bottom));
        const float cancel_right = final_rect.right - kSettingsSidePadding;
        const float save_right = cancel_right - cancel_button_width_ - kSettingsFooterButtonGap;
        const float reset_right = save_right - save_button_width_ - kSettingsFooterButtonGap;
        save_button_->Arrange(D2D1::RectF(reset_right, footer_top, save_right, footer_bottom));
        cancel_button_->Arrange(D2D1::RectF(save_right + kSettingsFooterButtonGap, footer_top, cancel_right, footer_bottom));

        // Content panel takes everything above the footer
        root_->Arrange(D2D1::RectF(final_rect.left, final_rect.top, final_rect.right, footer_top));
    }

    void Render(const UiDrawContext& context, UiRootState state) override
    {
        const UiDraw draw(context);
        draw.Clear(ui_theme::color::kWindowBackground);
        root_->Render(context, state);
        reset_button_->Render(context, state);
        save_button_->Render(context, state);
        cancel_button_->Render(context, state);
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
    void BuildUiTree()
    {
        // Title
        root_->AddItem(std::make_unique<Label>(
            UiMetadata(UiElementRole::Text, kUiActionNone, L"Settings", L"", L"settings-title", false, false),
            L"Settings", LabelStyle::Title), 17.0f);

        // Remember window size section (gap before = 16px)
        auto* section1 = root_->AddItem(std::make_unique<StackPanel>(
            UiMetadata(UiElementRole::Pane, kUiActionNone, L"", L"", L"", false, false)));
        section1->SetPadding(UiThickness{0.0f, 8.0f, 0.0f, 0.0f});
        section1->SetGap(3.0f);

        remember_checkbox_ = section1->AddItem(std::make_unique<Checkbox>(
            UiMetadata(UiElementRole::CheckBox, UiActionFromImgViewerAction(ImgViewerAction::None),
                L"Remember window size", L"Remember window size", L"remember-window-size"),
            L"Remember window size", draft_.remember_window_size));

        auto* indent1 = section1->AddItem(std::make_unique<StackPanel>(
            UiMetadata(UiElementRole::Pane, kUiActionNone, L"", L"", L"", false, false)));
        indent1->SetPadding(UiThickness{12.0f, 0.0f, 0.0f, 0.0f});
        indent1->SetGap(6.0f);

        indent1->AddItem(std::make_unique<Label>(
            UiMetadata(UiElementRole::Text, kUiActionNone, L"Window size", L"", L"window-size-label", false, false),
            L"Window size", LabelStyle::Muted));
        remember_radio_ = indent1->AddItem(std::make_unique<RadioButton>(
            UiMetadata(UiElementRole::RadioButton, UiActionFromImgViewerAction(ImgViewerAction::None),
                L"Remember last size", L"Remember last size", L"remember-last-size"),
            L"Remember last size", draft_.remember_window_size));
        default_radio_ = indent1->AddItem(std::make_unique<RadioButton>(
            UiMetadata(UiElementRole::RadioButton, UiActionFromImgViewerAction(ImgViewerAction::None),
                L"Use default size", L"Use default size", L"use-default-size"),
            L"Use default size", !draft_.remember_window_size));

        // Image rendering section (gap before = 16px)
        auto* section2 = root_->AddItem(std::make_unique<StackPanel>(
            UiMetadata(UiElementRole::Pane, kUiActionNone, L"", L"", L"", false, false)));
        section2->SetPadding(UiThickness{0.0f, 8.0f, 0.0f, 0.0f});
        section2->SetGap(6.0f);

        section2->AddItem(std::make_unique<Label>(
            UiMetadata(UiElementRole::Text, kUiActionNone, L"Image rendering", L"", L"image-rendering-label", false, false),
            L"Image rendering", LabelStyle::Muted));
        pixelated_checkbox_ = section2->AddItem(std::make_unique<Checkbox>(
            UiMetadata(UiElementRole::CheckBox, UiActionFromImgViewerAction(ImgViewerAction::None),
                L"Pixelated sampling", L"Pixelated sampling", L"pixelated-sampling"),
            L"Pixelated sampling", draft_.pixelated_sampling));
        checkerboard_checkbox_ = section2->AddItem(std::make_unique<Checkbox>(
            UiMetadata(UiElementRole::CheckBox, UiActionFromImgViewerAction(ImgViewerAction::None),
                L"Checkerboard background", L"Checkerboard background", L"checkerboard-background"),
            L"Checkerboard background", draft_.checkerboard_background));

        // Window frame section (gap before = 16px)
        auto* section3 = root_->AddItem(std::make_unique<StackPanel>(
            UiMetadata(UiElementRole::Pane, kUiActionNone, L"", L"", L"", false, false)));
        section3->SetPadding(UiThickness{0.0f, 8.0f, 0.0f, 0.0f});
        section3->SetGap(6.0f);

        section3->AddItem(std::make_unique<Label>(
            UiMetadata(UiElementRole::Text, kUiActionNone, L"Window frame", L"", L"window-frame-label", false, false),
            L"Window frame", LabelStyle::Muted));
        borderless_checkbox_ = section3->AddItem(std::make_unique<Checkbox>(
            UiMetadata(UiElementRole::CheckBox, UiActionFromImgViewerAction(ImgViewerAction::None),
                L"Borderless window", L"Borderless window", L"borderless-window"),
            L"Borderless window", draft_.borderless_window));

        // Opacity section (gap before = 16px)
        auto* section4 = root_->AddItem(std::make_unique<StackPanel>(
            UiMetadata(UiElementRole::Pane, kUiActionNone, L"", L"", L"", false, false)));
        section4->SetPadding(UiThickness{0.0f, 8.0f, 0.0f, 0.0f});
        section4->SetGap(4.0f);

        section4->AddItem(std::make_unique<Label>(
            UiMetadata(UiElementRole::Text, kUiActionNone, L"Opacity", L"", L"opacity-label", false, false),
            L"Opacity", LabelStyle::Muted));
        opacity_slider_row_ = section4->AddItem(std::make_unique<SliderRow>(
            UiMetadata(UiElementRole::Slider, UiActionFromImgViewerAction(ImgViewerAction::None),
                L"Opacity", L"Opacity", L"window-opacity"),
            kOpacityMinimum, kOpacityMaximum, draft_.window_opacity_percent,
            kOpacitySmallStep, kOpacityLargeStep));

        // Toolbar size section (gap before = 16px)
        auto* section5 = root_->AddItem(std::make_unique<StackPanel>(
            UiMetadata(UiElementRole::Pane, kUiActionNone, L"", L"", L"", false, false)));
        section5->SetPadding(UiThickness{0.0f, 8.0f, 0.0f, 0.0f});
        section5->SetGap(4.0f);

        section5->AddItem(std::make_unique<Label>(
            UiMetadata(UiElementRole::Text, kUiActionNone, L"Toolbar size", L"", L"toolbar-size-label", false, false),
            L"Toolbar size", LabelStyle::Muted));
        toolbar_scale_slider_row_ = section5->AddItem(std::make_unique<SliderRow>(
            UiMetadata(UiElementRole::Slider, UiActionFromImgViewerAction(ImgViewerAction::None),
                L"Toolbar size", L"Toolbar size", L"toolbar-size"),
            kToolbarScaleMinimum, kToolbarScaleMaximum, draft_.toolbar_scale_percent,
            kToolbarScaleSmallStep, kToolbarScaleLargeStep));

        // Shortcut filter section (gap before = 16px)
        auto* section6 = root_->AddItem(std::make_unique<StackPanel>(
            UiMetadata(UiElementRole::Pane, kUiActionNone, L"", L"", L"", false, false)));
        section6->SetPadding(UiThickness{0.0f, 8.0f, 0.0f, 0.0f});
        section6->SetGap(4.0f);

        section6->AddItem(std::make_unique<Label>(
            UiMetadata(UiElementRole::Text, kUiActionNone, L"Shortcut filter", L"", L"shortcut-filter-label", false, false),
            L"Shortcut filter", LabelStyle::Muted));
        filter_box_ = section6->AddItem(std::make_unique<TextBox>(
            UiMetadata(UiElementRole::Edit, UiActionFromImgViewerAction(ImgViewerAction::None),
                L"Shortcut filter", L"Shortcut filter", L"shortcut-filter"),
            L"Filter actions"));

        // Action shortcuts section (gap before = 24px)
        auto* section7 = root_->AddItem(std::make_unique<StackPanel>(
            UiMetadata(UiElementRole::Pane, kUiActionNone, L"", L"", L"", false, false)));
        section7->SetPadding(UiThickness{0.0f, 12.0f, 0.0f, 0.0f});
        section7->SetGap(8.0f);

        section7->AddItem(std::make_unique<Label>(
            UiMetadata(UiElementRole::Text, kUiActionNone, L"Action shortcuts", L"", L"action-shortcuts-label", false, false),
            L"Action shortcuts", LabelStyle::Muted));
        action_dropdown_ = section7->AddItem(std::make_unique<Dropdown>(
            UiMetadata(UiElementRole::ComboBox, UiActionFromImgViewerAction(ImgViewerAction::None),
                L"Action shortcuts", L"Action shortcuts", L"action-shortcuts"),
            BuildDropdownOptions()));
        shortcut_label_ = section7->AddItem(std::make_unique<Label>(
            UiMetadata(UiElementRole::Text, kUiActionNone, L"Shortcut", L"", L"shortcut-text", false, false),
            L"", LabelStyle::Body));

        // Footer buttons owned separately
        reset_button_ = static_cast<Button*>(root_owner_->AddChild(std::make_unique<Button>(
            UiMetadata(UiElementRole::Button, UiActionFromImgViewerAction(ImgViewerAction::ResetKeyBindings),
                L"Reset Shortcuts", L"Reset Shortcuts", L"reset-shortcuts"),
            kResetIcon, L"Reset")));
        save_button_ = static_cast<Button*>(root_owner_->AddChild(std::make_unique<Button>(
            UiMetadata(UiElementRole::Button, UiActionFromImgViewerAction(ImgViewerAction::SaveSettings),
                L"Save", L"Save", L"save-settings"),
            kSaveIcon, L"Save")));
        cancel_button_ = static_cast<Button*>(root_owner_->AddChild(std::make_unique<Button>(
            UiMetadata(UiElementRole::Button, UiActionFromImgViewerAction(ImgViewerAction::CloseSettings),
                L"Cancel", L"Cancel", L"cancel-settings"),
            kCancelIcon, L"Cancel")));
    }

    void ApplyElementEffect(UiElementId id) override
    {
        if (id == remember_checkbox_->Id()) { ToggleRememberWindowSize(); return; }
        if (id == remember_radio_->Id()) { SelectRememberWindowSize(); return; }
        if (id == default_radio_->Id()) { SelectDefaultWindowSize(); return; }
        if (id == pixelated_checkbox_->Id()) { TogglePixelatedSampling(); return; }
        if (id == checkerboard_checkbox_->Id()) { ToggleCheckerboardBackground(); return; }
        if (id == borderless_checkbox_->Id()) { ToggleBorderlessWindow(); return; }
        if (id == opacity_slider_row_->GetSlider()->Id()) { ApplyOpacitySlider(); return; }
        if (id == toolbar_scale_slider_row_->GetSlider()->Id()) { ApplyToolbarScaleSlider(); return; }
        if (id == action_dropdown_->Id()) { UpdateShortcutText(); return; }
    }

    void ToggleRememberWindowSize()
    {
        draft_.remember_window_size = !draft_.remember_window_size;
        SyncChoiceControls();
    }

    void SelectRememberWindowSize()
    {
        draft_.remember_window_size = true;
        SyncChoiceControls();
    }

    void SelectDefaultWindowSize()
    {
        draft_.remember_window_size = false;
        SyncChoiceControls();
    }

    void TogglePixelatedSampling()
    {
        draft_.pixelated_sampling = !draft_.pixelated_sampling;
        SyncChoiceControls();
    }

    void ToggleCheckerboardBackground()
    {
        draft_.checkerboard_background = !draft_.checkerboard_background;
        SyncChoiceControls();
    }

    void ToggleBorderlessWindow()
    {
        draft_.borderless_window = !draft_.borderless_window;
        SyncChoiceControls();
    }

    void ApplyOpacitySlider()
    {
        draft_.window_opacity_percent = ClampWindowOpacityPercent(opacity_slider_row_->Value());
        UpdateOpacityText();
        opacity_slider_row_->SetValueText(opacity_text_.c_str());
    }

    void ApplyToolbarScaleSlider()
    {
        draft_.toolbar_scale_percent = ClampToolbarScalePercent(toolbar_scale_slider_row_->Value());
        UpdateToolbarScaleText();
        toolbar_scale_slider_row_->SetValueText(toolbar_scale_text_.c_str());
    }

    void SyncChoiceControls()
    {
        remember_checkbox_->SetChecked(draft_.remember_window_size);
        remember_radio_->SetSelected(draft_.remember_window_size);
        default_radio_->SetSelected(!draft_.remember_window_size);
        pixelated_checkbox_->SetChecked(draft_.pixelated_sampling);
        checkerboard_checkbox_->SetChecked(draft_.checkerboard_background);
        borderless_checkbox_->SetChecked(draft_.borderless_window);
    }

    void UpdateShortcutText()
    {
        const ImgViewerAction action = ImgViewerActionFromUiAction(action_dropdown_->SelectedAction());
        shortcut_text_ = action != ImgViewerAction::None
            ? ShortcutsForAction(draft_.action_bindings, action)
            : std::wstring();
        shortcut_label_->SetText(shortcut_text_.c_str());
    }

    bool MatchesFilter(ImgViewerAction action) const
    {
        const std::wstring& filter = filter_box_->Text();
        if (filter.empty()) {
            return true;
        }
        std::wstring haystack = ImgViewerActionDisplayName(action);
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
        for (const ImgViewerActionInfo& action : ImgViewerActions()) {
            if (action.shown_in_settings && MatchesFilter(action.action)) {
                options.push_back(DropdownOption{action.display_name, action.action});
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
        if (opacity_slider_row_ != nullptr) {
            opacity_slider_row_->SetValueText(opacity_text_.c_str());
        }
    }

    void UpdateToolbarScaleText()
    {
        wchar_t text[16] = {};
        swprintf_s(text, L"%d%%", draft_.toolbar_scale_percent);
        toolbar_scale_text_ = text;
        if (toolbar_scale_slider_row_ != nullptr) {
            toolbar_scale_slider_row_->SetValueText(toolbar_scale_text_.c_str());
        }
    }

    ImgViewerConfig draft_;
    std::unique_ptr<StackPanel> root_owner_;
    StackPanel* root_ = nullptr;
    Checkbox* remember_checkbox_ = nullptr;
    Checkbox* pixelated_checkbox_ = nullptr;
    Checkbox* checkerboard_checkbox_ = nullptr;
    Checkbox* borderless_checkbox_ = nullptr;
    RadioButton* remember_radio_ = nullptr;
    RadioButton* default_radio_ = nullptr;
    SliderRow* opacity_slider_row_ = nullptr;
    SliderRow* toolbar_scale_slider_row_ = nullptr;
    Dropdown* action_dropdown_ = nullptr;
    TextBox* filter_box_ = nullptr;
    Label* shortcut_label_ = nullptr;
    Button* reset_button_ = nullptr;
    Button* save_button_ = nullptr;
    Button* cancel_button_ = nullptr;
    float reset_button_width_ = 69.0f;
    float save_button_width_ = 63.0f;
    float cancel_button_width_ = 64.0f;
    std::wstring opacity_text_;
    std::wstring toolbar_scale_text_;
    std::wstring shortcut_text_;
};

struct SettingsWindowContext final : public UiWindowDelegate {
    HWND owner = nullptr;
    ImgViewerContext* app = nullptr;
    UiWindowHost host;
    SettingsUi* ui = nullptr;
    int original_opacity_percent = 100;
    int original_toolbar_scale_percent = 125;
    bool saved = false;

    SettingsWindowContext(int original_opacity, int original_toolbar_scale) :
        original_opacity_percent(ClampWindowOpacityPercent(original_opacity)),
        original_toolbar_scale_percent(ClampToolbarScalePercent(original_toolbar_scale))
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
                SetImgViewerToolbarScale(owner, app, original_toolbar_scale_percent);
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
    if (config != nullptr && config->remember_window_size) {
        util::CaptureWindowSize(hwnd, &config->window_size.width, &config->window_size.height);
    }
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
        SetImgViewerToolbarScale(context->owner, context->app, context->app->config.toolbar_scale_percent);
        context->app->viewer.SetPixelatedSampling(context->app->config.pixelated_sampling);
        context->app->renderer.SetCheckerboardBackground(context->app->config.checkerboard_background);
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
            SetImgViewerToolbarScale(owner, app, ui->ToolbarScalePercent());
        }
        ApplyWindowOpacity(owner, ui->OpacityPercent());
    }
}

win32::WindowMessageResult SettingsWindowContext::OnUnhandledMessage(
    UiWindowHost& window_host,
    UINT message,
    WPARAM wparam,
    LPARAM lparam)
{
    switch (message) {
    case WM_GETMINMAXINFO:
        util::ApplyMinTrackSize(window_host.Hwnd(), lparam, kSettingsMinClientWidth, kSettingsMinClientHeight);
        return win32::WindowMessageResult::Handled();
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
    draft.toolbar_scale_percent = context->current_toolbar_scale_percent;
    auto* settings_context = new (std::nothrow) SettingsWindowContext(
        context->current_window_opacity_percent,
        context->current_toolbar_scale_percent);
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
                .style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
                .ex_style = WS_EX_DLGMODALFRAME,
                .width = kSettingsInitialWidth,
                .height = kSettingsInitialHeight,
                .owner = owner,
                .show_command = SW_SHOWNORMAL,
            },
            .action_message = kImgViewerUiActionMessage,
            .caret_timer_id = kCaretTimerId,
            .body_font_size = 9.5f,
            .icon_font_size = 11.0f,
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
