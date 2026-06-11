#include "imgviewer.settings.hpp"

#include <array>
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
#include "imgviewer.strings.hpp"
#include "imgviewer.ui.action.hpp"
#include "win32.util.hpp"
#include "ui.button.hpp"
#include "ui.draw.hpp"
#include "ui.label.hpp"
#include "ui.panel.hpp"
#include "ui.selection.hpp"
#include "ui.slider.hpp"
#include "ui.table.hpp"
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
constexpr int kEdgeClickZoneMinimum = 5;
constexpr int kEdgeClickZoneMaximum = 40;
constexpr int kEdgeClickZoneSmallStep = 1;
constexpr int kEdgeClickZoneLargeStep = 5;
constexpr int kSettingsInitialWidth = 500;
constexpr int kSettingsInitialHeight = 650;
constexpr int kSettingsMinClientWidth = 400;
constexpr int kSettingsMinClientHeight = 620;

constexpr float kSettingsSidePadding = 14.0f;
constexpr float kSettingsContentTopPadding = 9.0f;
constexpr float kSettingsFooterBottomPadding = 10.0f;
constexpr float kSettingsFooterButtonHeight = 24.0f;
constexpr float kSettingsFooterButtonWidth = 72.0f;
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
    return text.empty() ? ImgViewerString(ImgViewerStringId::NoShortcutConfigured) : text;
}


class SettingsUi final : public UiRoot {
private:
    using BoolField = bool ImgViewerConfig::*;
    using IntField = int ImgViewerConfig::*;
    using ClampIntFn = int (*)(int);

    struct BooleanSettingSpec final {
        ImgViewerStringId label = ImgViewerStringId::Empty;
        const wchar_t* automation_id = L"";
        BoolField field = nullptr;
        bool inverted = false;
    };

    struct BooleanControl final {
        const BooleanSettingSpec* spec = nullptr;
        Checkbox* checkbox = nullptr;
    };

    struct SliderSettingSpec final {
        ImgViewerStringId label = ImgViewerStringId::Empty;
        const wchar_t* automation_id = L"";
        IntField field = nullptr;
        int minimum = 0;
        int maximum = 100;
        int small_step = 1;
        int large_step = 10;
        ClampIntFn clamp = nullptr;
    };

    struct SliderControl final {
        const SliderSettingSpec* spec = nullptr;
        SliderRow* row = nullptr;
        std::wstring value_text;
    };

    static constexpr size_t kRememberWindowSizeSetting = 0;
    static constexpr size_t kPixelatedSamplingSetting = 1;
    static constexpr size_t kCheckerboardBackgroundSetting = 2;
    static constexpr size_t kBorderlessWindowSetting = 3;
    static constexpr size_t kEdgeClickNavigationSetting = 4;
    static constexpr std::array<BooleanSettingSpec, 5> kBooleanSettingSpecs{{
        {ImgViewerStringId::RememberWindowSize, L"remember-window-size", &ImgViewerConfig::remember_window_size},
        {ImgViewerStringId::PixelatedSampling, L"pixelated-sampling", &ImgViewerConfig::pixelated_sampling},
        {ImgViewerStringId::CheckerboardBackground, L"checkerboard-background", &ImgViewerConfig::checkerboard_background},
        {ImgViewerStringId::BorderlessWindow, L"borderless-window", &ImgViewerConfig::borderless_window},
        {ImgViewerStringId::EdgeClickNavigation, L"edge-click-navigation", &ImgViewerConfig::edge_click_navigation},
    }};

    static constexpr size_t kOpacitySliderSetting = 0;
    static constexpr size_t kToolbarScaleSliderSetting = 1;
    static constexpr size_t kEdgeClickZoneSliderSetting = 2;
    static constexpr std::array<SliderSettingSpec, 3> kSliderSettingSpecs{{
        {ImgViewerStringId::Opacity, L"window-opacity", &ImgViewerConfig::window_opacity_percent,
            kOpacityMinimum, kOpacityMaximum, kOpacitySmallStep, kOpacityLargeStep, ClampWindowOpacityPercent},
        {ImgViewerStringId::ToolbarSize, L"toolbar-size", &ImgViewerConfig::toolbar_scale_percent,
            kToolbarScaleMinimum, kToolbarScaleMaximum, kToolbarScaleSmallStep, kToolbarScaleLargeStep, ClampToolbarScalePercent},
        {ImgViewerStringId::EdgeClickZone, L"edge-click-zone", &ImgViewerConfig::edge_click_navigation_zone_percent,
            kEdgeClickZoneMinimum, kEdgeClickZoneMaximum, kEdgeClickZoneSmallStep, kEdgeClickZoneLargeStep,
            ClampEdgeClickNavigationZonePercent},
    }};

public:
    explicit SettingsUi(ImgViewerConfig config) : draft_(std::move(config))
    {
        auto scroll_panel = std::make_unique<ScrollPanel>(
            UiMetadata(
                UiElementRole::Pane,
                UiActionFromImgViewerAction(ImgViewerAction::None),
                ImgViewerString(ImgViewerStringId::Settings),
                ImgViewerString(ImgViewerStringId::Settings),
                L"settings-scroll-root",
                false,
                true));
        scroll_panel->SetScrollStep(42.0f);
        auto root_panel = std::make_unique<StackPanel>(
            UiRootMetadata(
                UiElementRole::Pane,
                UiActionFromImgViewerAction(ImgViewerAction::None),
                ImgViewerString(ImgViewerStringId::Settings),
                ImgViewerString(ImgViewerStringId::Settings),
                L"settings-root"));
        root_panel->SetPadding(UiThickness{kSettingsSidePadding, kSettingsContentTopPadding, kSettingsSidePadding, 0.0f});
        root_panel->SetGap(0.0f);
        root_ = root_panel.get();
        scroll_root_ = scroll_panel.get();
        scroll_root_->SetContent(std::move(root_panel));
        root_owner_ = std::move(scroll_panel);

        BuildUiTree();
        SyncChoiceControls();
        SyncSliderControls();
        UpdateFilterResults();
    }

    const ImgViewerConfig& Draft() const { return draft_; }
    int OpacityPercent() const { return draft_.window_opacity_percent; }
    int ToolbarScalePercent() const { return draft_.toolbar_scale_percent; }

    void SetOpacityPercent(int percent)
    {
        SetSliderValue(kOpacitySliderSetting, percent);
    }

    void SetToolbarScalePercent(int percent)
    {
        SetSliderValue(kToolbarScaleSliderSetting, percent);
    }

    UiElement* Root() override { return root_owner_.get(); }
    const UiElement* Root() const override { return root_owner_.get(); }
    const wchar_t* AccessibilityRootName() const override { return ImgViewerString(ImgViewerStringId::Settings); }

    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) override
    {
        const float footer_height = kSettingsFooterButtonHeight + kSettingsFooterBottomPadding;
        D2D1_SIZE_F content_available = D2D1::SizeF(available_size.width, available_size.height - footer_height);
        root_owner_->Measure(context, content_available);
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
            final_rect.left + kSettingsSidePadding + kSettingsFooterButtonWidth,
            footer_bottom));
        const float cancel_right = final_rect.right - kSettingsSidePadding;
        const float save_right = cancel_right - kSettingsFooterButtonWidth - kSettingsFooterButtonGap;
        const float reset_right = save_right - kSettingsFooterButtonWidth - kSettingsFooterButtonGap;
        save_button_->Arrange(D2D1::RectF(reset_right, footer_top, save_right, footer_bottom));
        cancel_button_->Arrange(D2D1::RectF(save_right + kSettingsFooterButtonGap, footer_top, cancel_right, footer_bottom));

        // Content panel takes everything above the footer
        root_owner_->Arrange(D2D1::RectF(final_rect.left, final_rect.top, final_rect.right, footer_top));
    }

    void Render(const UiDrawContext& context, UiRootState state) override
    {
        const UiDraw draw(context);
        draw.Clear(ui_theme::color::kWindowBackground);
        root_owner_->Render(context, state);
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
            return {};
        default:
            return {};
        }
    }

    UiEventResult OnPointerEvent(const UiPointerEvent& event) override
    {
        return root_owner_ != nullptr ? root_owner_->OnPointerEvent(event) : UiEventResult{};
    }
/// XXX: This should be removed later.
    UiElementId HitTest(D2D1_POINT_2F point) const override
    {
        if (cancel_button_ != nullptr && cancel_button_->Contains(point)) {
            return cancel_button_->Id();
        }
        if (save_button_ != nullptr && save_button_->Contains(point)) {
            return save_button_->Id();
        }
        if (reset_button_ != nullptr && reset_button_->Contains(point)) {
            return reset_button_->Id();
        }
        return UiRoot::HitTest(point);
    }

    UiEventResult OnKeyEvent(const UiKeyEvent& event) override
    {
        if (event.type != UiEventType::KeyDown) {
            return {};
        }
        const UINT virtual_key = event.virtual_key;
        if (virtual_key == VK_ESCAPE) {
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
        }
        return result;
    }

    UiEventResult ExecuteTextAction(UiAction action, HWND hwnd)
    {
        UiEventResult result = filter_box_->ExecuteEditAction(action, hwnd);
        if (result.handled) {
            UpdateFilterResults();
        }
        return result;
    }

private:
    static UiElementMetadata PaneMetadata()
    {
        return UiMetadata(UiElementRole::Pane, kUiActionNone, L"", L"", L"", false, false);
    }

    static UiElementMetadata LabelMetadata(const wchar_t* label, const wchar_t* automation_id)
    {
        return UiMetadata(UiElementRole::Text, kUiActionNone, label, L"", automation_id, false, false);
    }

    StackPanel* AddSection(ImgViewerStringId label, const wchar_t* automation_id, float gap = ui_theme::metrics::kStandardGap)
    {
        auto* section = root_->AddItem(std::make_unique<StackPanel>(PaneMetadata()));
        section->SetPadding(UiThickness{0.0f, ui_theme::metrics::kLargeGap, 0.0f, 0.0f});
        section->SetGap(gap);
        section->AddItem(std::make_unique<Label>(
            LabelMetadata(ImgViewerString(label), automation_id),
            ImgViewerString(label),
            LabelStyle::Muted));
        return section;
    }

    Checkbox* AddCheckboxSetting(StackPanel* section, size_t index)
    {
        const BooleanSettingSpec& spec = kBooleanSettingSpecs[index];
        auto checkbox = std::make_unique<Checkbox>(
            UiMetadata(
                UiElementRole::CheckBox,
                UiActionFromImgViewerAction(ImgViewerAction::None),
                ImgViewerString(spec.label),
                ImgViewerString(spec.label),
                spec.automation_id),
            ImgViewerString(spec.label),
            BooleanSettingValue(spec));
        Checkbox* result = section->AddItem(std::move(checkbox));
        boolean_controls_[index] = BooleanControl{.spec = &spec, .checkbox = result};
        return result;
    }

    SliderRow* AddSliderSetting(StackPanel* section, size_t index)
    {
        const SliderSettingSpec& spec = kSliderSettingSpecs[index];
        auto slider = std::make_unique<SliderRow>(
            UiMetadata(
                UiElementRole::Slider,
                UiActionFromImgViewerAction(ImgViewerAction::None),
                ImgViewerString(spec.label),
                ImgViewerString(spec.label),
                spec.automation_id),
            spec.minimum,
            spec.maximum,
            draft_.*(spec.field),
            spec.small_step,
            spec.large_step);
        SliderRow* result = section->AddItem(std::move(slider));
        slider_controls_[index] = SliderControl{.spec = &spec, .row = result};
        result->GetSlider()->SetAccessibilityValueChangedHandler([this, index](int value) {
            SetSliderValue(index, value);
        });
        UpdateSliderText(slider_controls_[index]);
        return result;
    }

    static size_t LanguageIndex(ImgViewerLanguage language)
    {
        return language == ImgViewerLanguage::SimplifiedChinese ? 1 : 0;
    }

    static ImgViewerLanguage LanguageFromIndex(size_t index)
    {
        return index == 1 ? ImgViewerLanguage::SimplifiedChinese : ImgViewerLanguage::English;
    }

    Dropdown* AddLanguageDropdown(StackPanel* section)
    {
        auto dropdown = std::make_unique<Dropdown>(
            UiMetadata(
                UiElementRole::ComboBox,
                UiActionFromImgViewerAction(ImgViewerAction::None),
                ImgViewerString(ImgViewerStringId::Language),
                ImgViewerString(ImgViewerStringId::Language),
                L"language"),
            std::vector<DropdownOption>{
                DropdownOption{ImgViewerString(ImgViewerStringId::EnglishLanguage), kUiActionNone},
                DropdownOption{ImgViewerString(ImgViewerStringId::SimplifiedChineseLanguage), kUiActionNone},
            });
        dropdown->SetSelectedIndex(LanguageIndex(draft_.language));
        return section->AddItem(std::move(dropdown));
    }

    Button* AddFooterButton(ImgViewerAction action, const wchar_t* name, const wchar_t* automation_id, const wchar_t* icon, const wchar_t* text)
    {
        return static_cast<Button*>(root_owner_->AddChild(std::make_unique<Button>(
            UiMetadata(UiElementRole::Button, UiActionFromImgViewerAction(action), name, name, automation_id),
            icon,
            text)));
    }

    void BuildUiTree()
    {
        // Title
        root_->AddItem(std::make_unique<Label>(
            UiMetadata(UiElementRole::Text, kUiActionNone, ImgViewerString(ImgViewerStringId::Settings), L"", L"settings-title", false, false),
            ImgViewerString(ImgViewerStringId::Settings), LabelStyle::Title), 17.0f);

        StackPanel* language_section = AddSection(ImgViewerStringId::Language, L"language-label", ui_theme::metrics::kSmallGap);
        language_dropdown_ = AddLanguageDropdown(language_section);

        StackPanel* window_size_section = AddSection(ImgViewerStringId::WindowSize, L"window-size-label", 3.0f);
        AddCheckboxSetting(window_size_section, kRememberWindowSizeSetting);
        remember_radio_ = window_size_section->AddItem(std::make_unique<RadioButton>(
            UiMetadata(UiElementRole::RadioButton, UiActionFromImgViewerAction(ImgViewerAction::None),
                ImgViewerString(ImgViewerStringId::RememberLastSize), ImgViewerString(ImgViewerStringId::RememberLastSize), L"remember-last-size"),
            ImgViewerString(ImgViewerStringId::RememberLastSize), draft_.remember_window_size));
        default_radio_ = window_size_section->AddItem(std::make_unique<RadioButton>(
            UiMetadata(UiElementRole::RadioButton, UiActionFromImgViewerAction(ImgViewerAction::None),
                ImgViewerString(ImgViewerStringId::UseDefaultSize), ImgViewerString(ImgViewerStringId::UseDefaultSize), L"use-default-size"),
            ImgViewerString(ImgViewerStringId::UseDefaultSize), !draft_.remember_window_size));

        StackPanel* image_section = AddSection(ImgViewerStringId::ImageRendering, L"image-rendering-label");
        image_section->AddItem(std::make_unique<Label>(
            LabelMetadata(ImgViewerString(ImgViewerStringId::InitialImageView), L"initial-image-view-label"),
            ImgViewerString(ImgViewerStringId::InitialImageView),
            LabelStyle::Muted));
        initial_fit_window_radio_ = image_section->AddItem(std::make_unique<RadioButton>(
            UiMetadata(UiElementRole::RadioButton, UiActionFromImgViewerAction(ImgViewerAction::None),
                ImgViewerString(ImgViewerStringId::FitWindow), ImgViewerString(ImgViewerStringId::FitWindow), L"initial-fit-window"),
            ImgViewerString(ImgViewerStringId::FitWindow), draft_.initial_image_view_mode == InitialImageViewMode::FitWindow));
        initial_actual_size_radio_ = image_section->AddItem(std::make_unique<RadioButton>(
            UiMetadata(UiElementRole::RadioButton, UiActionFromImgViewerAction(ImgViewerAction::None),
                ImgViewerString(ImgViewerStringId::ActualSize), ImgViewerString(ImgViewerStringId::ActualSize), L"initial-actual-size"),
            ImgViewerString(ImgViewerStringId::ActualSize), draft_.initial_image_view_mode == InitialImageViewMode::ActualSize));
        AddCheckboxSetting(image_section, kPixelatedSamplingSetting);
        AddCheckboxSetting(image_section, kCheckerboardBackgroundSetting);

        StackPanel* frame_section = AddSection(ImgViewerStringId::WindowFrame, L"window-frame-label");
        AddCheckboxSetting(frame_section, kBorderlessWindowSetting);

        StackPanel* opacity_section = AddSection(ImgViewerStringId::Opacity, L"opacity-label", ui_theme::metrics::kSmallGap);
        AddSliderSetting(opacity_section, kOpacitySliderSetting);

        StackPanel* toolbar_section = AddSection(ImgViewerStringId::ToolbarSize, L"toolbar-size-label", ui_theme::metrics::kSmallGap);
        AddSliderSetting(toolbar_section, kToolbarScaleSliderSetting);

        StackPanel* navigation_section = AddSection(ImgViewerStringId::Navigation, L"navigation-label", ui_theme::metrics::kSmallGap);
        AddCheckboxSetting(navigation_section, kEdgeClickNavigationSetting);
        AddSliderSetting(navigation_section, kEdgeClickZoneSliderSetting);

        StackPanel* filter_section = AddSection(ImgViewerStringId::ShortcutFilter, L"shortcut-filter-label", ui_theme::metrics::kSmallGap);
        filter_box_ = filter_section->AddItem(std::make_unique<TextBox>(
            UiMetadata(UiElementRole::Edit, UiActionFromImgViewerAction(ImgViewerAction::None),
                ImgViewerString(ImgViewerStringId::ShortcutFilter), ImgViewerString(ImgViewerStringId::ShortcutFilter), L"shortcut-filter"),
            ImgViewerString(ImgViewerStringId::FilterActions)));

        StackPanel* shortcuts_section = AddSection(ImgViewerStringId::ActionShortcuts, L"action-shortcuts-label", 8.0f);
        action_table_ = shortcuts_section->AddItem(std::make_unique<Table>(
            UiMetadata(UiElementRole::Pane, UiActionFromImgViewerAction(ImgViewerAction::None),
                ImgViewerString(ImgViewerStringId::ActionShortcuts), ImgViewerString(ImgViewerStringId::ActionShortcuts), L"action-shortcuts")));
        action_table_->SetColumns(std::vector<TableColumn>{
            TableColumn{ImgViewerString(ImgViewerStringId::Action), 170.0f, false},
            TableColumn{ImgViewerString(ImgViewerStringId::Shortcut), 0.0f, true},
        });
        action_table_->SetHeaderVisible(true);
        action_table_->SetSelectionEnabled(true);
        action_table_->SetRowHeight(21.0f);

        reset_button_ = AddFooterButton(ImgViewerAction::ResetKeyBindings, ImgViewerString(ImgViewerStringId::ResetShortcuts), L"reset-shortcuts", kResetIcon, ImgViewerString(ImgViewerStringId::Reset));
        save_button_ = AddFooterButton(ImgViewerAction::SaveSettings, ImgViewerString(ImgViewerStringId::Save), L"save-settings", kSaveIcon, ImgViewerString(ImgViewerStringId::Save));
        cancel_button_ = AddFooterButton(ImgViewerAction::CloseSettings, ImgViewerString(ImgViewerStringId::Cancel), L"cancel-settings", kCancelIcon, ImgViewerString(ImgViewerStringId::Cancel));
    }

    void ApplyElementEffect(UiElementId id) override
    {
        if (id == remember_radio_->Id()) { SelectRememberWindowSize(); return; }
        if (id == default_radio_->Id()) { SelectDefaultWindowSize(); return; }
        if (id == initial_fit_window_radio_->Id()) { SelectInitialFitWindow(); return; }
        if (id == initial_actual_size_radio_->Id()) { SelectInitialActualSize(); return; }
        if (ApplyBooleanSetting(id)) { return; }
        if (ApplySliderSetting(id)) { return; }
        if (ApplyLanguageSetting(id)) { return; }
        if (id == action_table_->Id()) { return; }
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

    void SelectInitialFitWindow()
    {
        draft_.initial_image_view_mode = InitialImageViewMode::FitWindow;
        SyncChoiceControls();
    }

    void SelectInitialActualSize()
    {
        draft_.initial_image_view_mode = InitialImageViewMode::ActualSize;
        SyncChoiceControls();
    }

    void SyncChoiceControls()
    {
        for (BooleanControl& control : boolean_controls_) {
            if (control.checkbox != nullptr && control.spec != nullptr) {
                control.checkbox->SetChecked(BooleanSettingValue(*control.spec));
            }
        }
        remember_radio_->SetSelected(draft_.remember_window_size);
        default_radio_->SetSelected(!draft_.remember_window_size);
        initial_fit_window_radio_->SetSelected(
            draft_.initial_image_view_mode == InitialImageViewMode::FitWindow);
        initial_actual_size_radio_->SetSelected(
            draft_.initial_image_view_mode == InitialImageViewMode::ActualSize);
        if (language_dropdown_ != nullptr) {
            language_dropdown_->SetSelectedIndex(LanguageIndex(draft_.language));
        }
    }

    bool BooleanSettingValue(const BooleanSettingSpec& spec) const
    {
        const bool value = draft_.*(spec.field);
        return spec.inverted ? !value : value;
    }

    bool ApplyBooleanSetting(UiElementId id)
    {
        for (const BooleanControl& control : boolean_controls_) {
            if (control.checkbox != nullptr && control.checkbox->Id() == id) {
                draft_.*(control.spec->field) = !(draft_.*(control.spec->field));
                SyncChoiceControls();
                return true;
            }
        }
        return false;
    }

    bool ApplyLanguageSetting(UiElementId id)
    {
        if (language_dropdown_ == nullptr || language_dropdown_->Id() != id) {
            return false;
        }
        draft_.language = LanguageFromIndex(language_dropdown_->SelectedIndex());
        SyncChoiceControls();
        return true;
    }

    void SyncSliderControls()
    {
        for (SliderControl& control : slider_controls_) {
            if (control.row != nullptr && control.spec != nullptr) {
                SetSliderValue(control, draft_.*(control.spec->field));
            }
        }
    }

    bool ApplySliderSetting(UiElementId id)
    {
        if (SliderControl* control = SliderControlForId(id)) {
            SetSliderValue(*control, control->row->Value());
            return true;
        }
        return false;
    }

    SliderControl* SliderControlForId(UiElementId id)
    {
        for (SliderControl& control : slider_controls_) {
            if (control.row != nullptr && control.row->GetSlider()->Id() == id) {
                return &control;
            }
        }
        return nullptr;
    }

    void SetSliderValue(size_t index, int value)
    {
        SetSliderValue(slider_controls_[index], value);
    }

    void SetSliderValue(SliderControl& control, int value)
    {
        const int clamped = control.spec->clamp(value);
        draft_.*(control.spec->field) = clamped;
        control.row->SetValue(clamped);
        UpdateSliderText(control);
    }

    void UpdateSliderText(SliderControl& control)
    {
        wchar_t text[16] = {};
        swprintf_s(text, L"%d%%", draft_.*(control.spec->field));
        control.value_text = text;
        if (control.row != nullptr) {
            control.row->SetValueText(control.value_text.c_str());
        }
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

    std::vector<TableRow> BuildShortcutRows() const
    {
        std::vector<TableRow> rows;
        for (const ImgViewerActionInfo& action : ImgViewerActions()) {
            if (action.shown_in_settings && MatchesFilter(action.action)) {
                rows.push_back(TableRow{
                    .cells = {ImgViewerString(action.display_name), ShortcutsForAction(draft_.action_bindings, action.action)},
                    .action = UiActionFromImgViewerAction(action.action),
                });
            }
        }
        if (rows.empty()) {
            rows.push_back(TableRow{
                .cells = {ImgViewerString(ImgViewerStringId::NoMatches), L""},
                .enabled = false,
            });
        }
        return rows;
    }

    void UpdateFilterResults()
    {
        action_table_->SetRows(BuildShortcutRows());
    }

    ImgViewerConfig draft_;
    std::unique_ptr<ScrollPanel> root_owner_;
    ScrollPanel* scroll_root_ = nullptr;
    StackPanel* root_ = nullptr;
    std::array<BooleanControl, kBooleanSettingSpecs.size()> boolean_controls_{};
    std::array<SliderControl, kSliderSettingSpecs.size()> slider_controls_{};
    RadioButton* remember_radio_ = nullptr;
    RadioButton* default_radio_ = nullptr;
    RadioButton* initial_fit_window_radio_ = nullptr;
    RadioButton* initial_actual_size_radio_ = nullptr;
    Dropdown* language_dropdown_ = nullptr;
    TextBox* filter_box_ = nullptr;
    Table* action_table_ = nullptr;
    Button* reset_button_ = nullptr;
    Button* save_button_ = nullptr;
    Button* cancel_button_ = nullptr;
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
        const bool language_changed = context->app->config.language != draft.language;
        context->app->config = draft;
        SetImgViewerLanguage(context->app->config.language);
        context->app->current_window_opacity_percent = context->app->config.window_opacity_percent;
        ApplyWindowOpacity(context->owner, context->app->current_window_opacity_percent);
        SetImgViewerToolbarScale(context->owner, context->app, context->app->config.toolbar_scale_percent);
        context->app->viewer.SetPixelatedSampling(context->app->config.pixelated_sampling);
        context->app->renderer.SetCheckerboardBackground(context->app->config.checkerboard_background);
        SaveImgViewerConfig(context->app->config);
        if (language_changed) {
            ResetImgViewerUi(context->owner, context->app);
        }
        if (frame_changed) {
            ApplyImgViewerWindowFrame(context->owner, context->app, true);
        } else if (!language_changed) {
            InvalidateRect(context->owner, nullptr, FALSE);
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
                .title = ImgViewerString(ImgViewerStringId::Settings),
                .style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
                .ex_style = WS_EX_DLGMODALFRAME | WS_EX_NOREDIRECTIONBITMAP,
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
        settings_context,
        &context->graphics_device);
    if (FAILED(create_hr)) {
        context->settings_context = nullptr;
        delete settings_context;
        RETURN_IF_FAILED(create_hr);
    }

    settings_context->host.Window().Show(SW_SHOWNORMAL);
    context->interaction.SetModal(ImgViewerModalOwner::Settings);
    return S_OK;
}

void CleanupImgViewerSettingsWindow(ImgViewerContext* context, void* settings_context)
{
    if (context != nullptr && context->settings_context == settings_context) {
        context->settings_context = nullptr;
        context->interaction.ClearModal(ImgViewerModalOwner::Settings);
    }
    delete static_cast<SettingsWindowContext*>(settings_context);
}
