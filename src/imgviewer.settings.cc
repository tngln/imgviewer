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

#include "experimental/ui.decl.hpp"
#include "experimental/util.signal.hpp"
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
#include "ui.panel.hpp"
#include "ui.selection.hpp"
#include "ui.slider.hpp"
#include "ui.table.hpp"
#include "ui.textbox.hpp"
#include "ui.theme.hpp"
#include "ui.window.hpp"

namespace {
namespace ui_decl = experimental::ui_decl;

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

struct SettingsState final {
    ActionBindings action_bindings;
    InitialImageViewMode initial_image_view_mode = InitialImageViewMode::FitWindow;
    util::Signal<bool> remember_window_size;
    util::Signal<bool> pixelated_sampling;
    util::Signal<bool> checkerboard_background;
    util::Signal<bool> borderless_window;
    util::Signal<bool> edge_click_navigation;
    util::Signal<int> window_opacity_percent;
    util::Signal<int> toolbar_scale_percent;
    util::Signal<int> edge_click_navigation_zone_percent;
    util::Signal<int> language_index_signal;

    SettingsState(
        ActionBindings bindings,
        InitialImageViewMode image_view_mode,
        util::Signal<bool> remember_window_size_value,
        util::Signal<bool> pixelated_sampling_value,
        util::Signal<bool> checkerboard_background_value,
        util::Signal<bool> borderless_window_value,
        util::Signal<bool> edge_click_navigation_value,
        util::Signal<int> window_opacity_percent_value,
        util::Signal<int> toolbar_scale_percent_value,
        util::Signal<int> edge_click_navigation_zone_percent_value,
        util::Signal<int> language_index) :
        action_bindings(std::move(bindings)),
        initial_image_view_mode(image_view_mode),
        remember_window_size(std::move(remember_window_size_value)),
        pixelated_sampling(std::move(pixelated_sampling_value)),
        checkerboard_background(std::move(checkerboard_background_value)),
        borderless_window(std::move(borderless_window_value)),
        edge_click_navigation(std::move(edge_click_navigation_value)),
        window_opacity_percent(std::move(window_opacity_percent_value)),
        toolbar_scale_percent(std::move(toolbar_scale_percent_value)),
        edge_click_navigation_zone_percent(std::move(edge_click_navigation_zone_percent_value)),
        language_index_signal(std::move(language_index))
    {
    }
};


class SettingsUi final : public UiRoot {
private:
    using BoolField = bool ImgViewerConfig::*;
    using IntField = int ImgViewerConfig::*;
    using ClampIntFn = int (*)(int);

    struct BooleanSettingSpec final {
        ImgViewerStringId label = ImgViewerStringId::Empty;
        BoolField field = nullptr;
        bool inverted = false;
    };

    struct BooleanControl final {
        const BooleanSettingSpec* spec = nullptr;
        Checkbox* checkbox = nullptr;
    };

    struct SliderSettingSpec final {
        ImgViewerStringId label = ImgViewerStringId::Empty;
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

    struct ViewRefs final {
        BooleanControl remember_window_size;
        BooleanControl pixelated_sampling;
        BooleanControl checkerboard_background;
        BooleanControl borderless_window;
        BooleanControl edge_click_navigation;
        SliderControl opacity;
        SliderControl toolbar_scale;
        SliderControl edge_click_zone;
        RadioButton* remember_radio = nullptr;
        RadioButton* default_radio = nullptr;
        RadioButton* initial_fit_window_radio = nullptr;
        RadioButton* initial_actual_size_radio = nullptr;
        Dropdown* language_dropdown = nullptr;
        TextBox* filter_box = nullptr;
        Table* action_table = nullptr;
        Button* reset_button = nullptr;
        Button* save_button = nullptr;
        Button* cancel_button = nullptr;
    };

    static constexpr size_t kRememberWindowSizeSetting = 0;
    static constexpr size_t kPixelatedSamplingSetting = 1;
    static constexpr size_t kCheckerboardBackgroundSetting = 2;
    static constexpr size_t kBorderlessWindowSetting = 3;
    static constexpr size_t kEdgeClickNavigationSetting = 4;
    static constexpr std::array<BooleanSettingSpec, 5> kBooleanSettingSpecs{{
        {ImgViewerStringId::RememberWindowSize, &ImgViewerConfig::remember_window_size},
        {ImgViewerStringId::PixelatedSampling, &ImgViewerConfig::pixelated_sampling},
        {ImgViewerStringId::CheckerboardBackground, &ImgViewerConfig::checkerboard_background},
        {ImgViewerStringId::BorderlessWindow, &ImgViewerConfig::borderless_window},
        {ImgViewerStringId::EdgeClickNavigation, &ImgViewerConfig::edge_click_navigation},
    }};

    static constexpr size_t kOpacitySliderSetting = 0;
    static constexpr size_t kToolbarScaleSliderSetting = 1;
    static constexpr size_t kEdgeClickZoneSliderSetting = 2;
    static constexpr std::array<SliderSettingSpec, 3> kSliderSettingSpecs{{
        {ImgViewerStringId::Opacity, &ImgViewerConfig::window_opacity_percent,
            kOpacityMinimum, kOpacityMaximum, kOpacitySmallStep, kOpacityLargeStep, ClampWindowOpacityPercent},
        {ImgViewerStringId::ToolbarSize, &ImgViewerConfig::toolbar_scale_percent,
            kToolbarScaleMinimum, kToolbarScaleMaximum, kToolbarScaleSmallStep, kToolbarScaleLargeStep, ClampToolbarScalePercent},
        {ImgViewerStringId::EdgeClickZone, &ImgViewerConfig::edge_click_navigation_zone_percent,
            kEdgeClickZoneMinimum, kEdgeClickZoneMaximum, kEdgeClickZoneSmallStep, kEdgeClickZoneLargeStep,
            ClampEdgeClickNavigationZonePercent},
    }};

    static bool BooleanSettingValue(const ImgViewerConfig& config, const BooleanSettingSpec& spec)
    {
        const bool value = config.*(spec.field);
        return spec.inverted ? !value : value;
    }

    static SettingsState CreateState(ImgViewerConfig config)
    {
        return SettingsState(
            std::move(config.action_bindings),
            config.initial_image_view_mode,
            util::Signal<bool>(BooleanSettingValue(config, kBooleanSettingSpecs[kRememberWindowSizeSetting])),
            util::Signal<bool>(BooleanSettingValue(config, kBooleanSettingSpecs[kPixelatedSamplingSetting])),
            util::Signal<bool>(BooleanSettingValue(config, kBooleanSettingSpecs[kCheckerboardBackgroundSetting])),
            util::Signal<bool>(BooleanSettingValue(config, kBooleanSettingSpecs[kBorderlessWindowSetting])),
            util::Signal<bool>(BooleanSettingValue(config, kBooleanSettingSpecs[kEdgeClickNavigationSetting])),
            util::Signal<int>(config.*(kSliderSettingSpecs[kOpacitySliderSetting].field)),
            util::Signal<int>(config.*(kSliderSettingSpecs[kToolbarScaleSliderSetting].field)),
            util::Signal<int>(config.*(kSliderSettingSpecs[kEdgeClickZoneSliderSetting].field)),
            util::Signal<int>(static_cast<int>(LanguageIndex(config.language))));
    }

public:
    explicit SettingsUi(ImgViewerConfig config) :
        state_(CreateState(std::move(config)))
    {
        auto scroll_panel = std::make_unique<ScrollPanel>(
            UiMetadata(UiElementRole::Pane, ImgViewerString(ImgViewerStringId::Settings), kUiTooltipFromName, false, true));
        scroll_panel->SetScrollStep(42.0f);
        auto root_panel = std::make_unique<StackPanel>(
            UiRootMetadata(UiElementRole::Pane, ImgViewerString(ImgViewerStringId::Settings), kUiTooltipFromName));
        root_panel->SetPadding(UiThickness{kSettingsSidePadding, kSettingsContentTopPadding, kSettingsSidePadding, kSettingsFooterBottomPadding});
        root_panel->SetGap(0.0f);
        root_ = root_panel.get();
        scroll_root_ = scroll_panel.get();
        scroll_root_->SetContent(std::move(root_panel));
        root_owner_ = std::move(scroll_panel);

        BuildUiTree();
        ConnectSignals();
        UpdateFilterResults();
    }

    ImgViewerConfig Draft() const
    {
        ImgViewerConfig config;
        config.language = LanguageFromIndex(static_cast<size_t>(state_.language_index_signal.Get()));
        config.initial_image_view_mode = state_.initial_image_view_mode;
        config.remember_window_size = state_.remember_window_size.Get();
        config.pixelated_sampling = state_.pixelated_sampling.Get();
        config.checkerboard_background = state_.checkerboard_background.Get();
        config.borderless_window = state_.borderless_window.Get();
        config.edge_click_navigation = state_.edge_click_navigation.Get();
        config.window_opacity_percent = ClampWindowOpacityPercent(state_.window_opacity_percent.Get());
        config.toolbar_scale_percent = ClampToolbarScalePercent(state_.toolbar_scale_percent.Get());
        config.edge_click_navigation_zone_percent = ClampEdgeClickNavigationZonePercent(state_.edge_click_navigation_zone_percent.Get());
        config.action_bindings = state_.action_bindings;
        return config;
    }
    int OpacityPercent() const { return state_.window_opacity_percent.Get(); }
    int ToolbarScalePercent() const { return state_.toolbar_scale_percent.Get(); }

    void SetOpacityPercent(int percent)
    {
        SetSliderValue(view_.opacity, state_.window_opacity_percent, percent);
    }

    void SetToolbarScalePercent(int percent)
    {
        SetSliderValue(view_.toolbar_scale, state_.toolbar_scale_percent, percent);
    }

    UiElement* Root() override { return root_owner_.get(); }
    const UiElement* Root() const override { return root_owner_.get(); }
    const wchar_t* AccessibilityRootName() const override { return ImgViewerString(ImgViewerStringId::Settings); }

    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) override
    {
        root_owner_->Measure(context, available_size);
        return available_size;
    }

    void Arrange(D2D1_RECT_F final_rect) override
    {
        root_owner_->Arrange(final_rect);
    }

    void Render(const UiDrawContext& context, UiRootState state) override
    {
        const UiDraw draw(context);
        draw.Clear(ui_theme::color::kWindowBackground);
        root_owner_->Render(context, state);
    }

    UiEventResult OnInputEvent(const UiInputEvent& event) override
    {
        switch (event.type) {
        case UiEventType::TextChar:
            return OnTextChar(event.focused, event.character);
        case UiEventType::ImeStartComposition:
            return event.focused == view_.filter_box->Id() ? UiEventResult{.handled = true, .wants_ime_position = true} : UiEventResult{};
        case UiEventType::ImeComposition:
            return event.focused == view_.filter_box->Id() ? view_.filter_box->OnInputEvent(event) : UiEventResult{};
        case UiEventType::ImeEndComposition:
            return event.focused == view_.filter_box->Id() ? view_.filter_box->OnInputEvent(event) : UiEventResult{};
        case UiEventType::Timer:
            if (event.focused == view_.filter_box->Id()) {
                return view_.filter_box->OnInputEvent(event);
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
        if (focused != view_.filter_box->Id()) {
            return {};
        }
        UiEventResult result = view_.filter_box->OnInputEvent(UiInputEvent{.type = UiEventType::TextChar, .character = ch});
        if (result.handled) {
            UpdateFilterResults();
        }
        return result;
    }

    UiEventResult ExecuteTextAction(UiAction action, HWND hwnd)
    {
        UiEventResult result = view_.filter_box->ExecuteEditAction(action, hwnd);
        if (result.handled) {
            UpdateFilterResults();
        }
        return result;
    }

private:
    StackPanel* AddSection(ImgViewerStringId label, float gap = ui_theme::metrics::kStandardGap)
    {
        auto content = ui_decl::VStack();
        content->SetGap(gap);
        StackPanel* section = content.get();
        root_->AddItem(ui_decl::Section(ImgViewerString(label), std::move(content)));
        return section;
    }

    Checkbox* AddCheckboxSetting(
        StackPanel* section,
        const BooleanSettingSpec& spec,
        util::Signal<bool>& signal,
        BooleanControl* control_slot)
    {
        auto checkbox = ui_decl::Toggle(
            ImgViewerString(spec.label),
            signal.Get());
        Checkbox* result = section->AddItem(std::move(checkbox));
        if (control_slot != nullptr) {
            *control_slot = BooleanControl{.spec = &spec, .checkbox = result};
        }
        result->SetOnToggled([&signal](bool checked) {
            signal.Set(checked);
        });
        return result;
    }

    SliderRow* AddSliderSetting(
        StackPanel* section,
        const SliderSettingSpec& spec,
        util::Signal<int>& signal,
        SliderControl* control_slot)
    {
        auto slider = ui_decl::SliderField(
            ImgViewerString(spec.label),
            spec.minimum,
            spec.maximum,
            signal.Get(),
            spec.small_step,
            spec.large_step);
        SliderRow* result = section->AddItem(std::move(slider));
        if (control_slot != nullptr) {
            *control_slot = SliderControl{.spec = &spec, .row = result};
        }
        result->GetSlider()->SetOnValueChanged([this, &signal, control_slot](int value) {
            SetSliderValue(*control_slot, signal, value);
        });
        result->GetSlider()->SetAccessibilityValueChangedHandler([this, &signal, control_slot](int value) {
            SetSliderValue(*control_slot, signal, value);
        });
        UpdateSliderText(*control_slot, signal.Get());
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
            UiMetadata(UiElementRole::ComboBox, ImgViewerString(ImgViewerStringId::Language), kUiTooltipFromName),
            std::vector<DropdownOption>{
                DropdownOption{ImgViewerString(ImgViewerStringId::EnglishLanguage), kUiActionNone},
                DropdownOption{ImgViewerString(ImgViewerStringId::SimplifiedChineseLanguage), kUiActionNone},
            });
        dropdown->SetSelectedIndex(static_cast<size_t>(state_.language_index_signal.Get()));
        Dropdown* result = section->AddItem(std::move(dropdown));
        result->SetOnSelectionChanged([this](size_t index) {
            state_.language_index_signal.Set(static_cast<int>(index));
        });
        return result;
    }

    Button* AddFooterButton(ImgViewerAction action, const wchar_t* name, const wchar_t* icon, const wchar_t* text)
    {
        return footer_panel_ != nullptr ? footer_panel_->AddItem(
            ui_decl::ActionButton(UiActionFromImgViewerAction(action), name, icon, text),
            kSettingsFooterButtonWidth) : nullptr;
    }

    void BuildUiTree()
    {
        root_->AddItem(ui_decl::Title(ImgViewerString(ImgViewerStringId::Settings)), 17.0f);

        StackPanel* language_section = AddSection(ImgViewerStringId::Language, ui_theme::metrics::kSmallGap);
        view_.language_dropdown = AddLanguageDropdown(language_section);

        StackPanel* window_size_section = AddSection(ImgViewerStringId::WindowSize, 3.0f);
        AddCheckboxSetting(window_size_section, kBooleanSettingSpecs[kRememberWindowSizeSetting], state_.remember_window_size, &view_.remember_window_size);
        view_.remember_radio = window_size_section->AddItem(
            ui_decl::Radio(ImgViewerString(ImgViewerStringId::RememberLastSize), state_.remember_window_size.Get()));
        view_.default_radio = window_size_section->AddItem(
            ui_decl::Radio(ImgViewerString(ImgViewerStringId::UseDefaultSize), !state_.remember_window_size.Get()));
        view_.remember_radio->SetOnSelected([this]() { SelectRememberWindowSize(); });
        view_.default_radio->SetOnSelected([this]() { SelectDefaultWindowSize(); });

        StackPanel* image_section = AddSection(ImgViewerStringId::ImageRendering);
        image_section->AddItem(ui_decl::Muted(ImgViewerString(ImgViewerStringId::InitialImageView)));
        view_.initial_fit_window_radio = image_section->AddItem(
            ui_decl::Radio(ImgViewerString(ImgViewerStringId::FitWindow), state_.initial_image_view_mode == InitialImageViewMode::FitWindow));
        view_.initial_actual_size_radio = image_section->AddItem(
            ui_decl::Radio(ImgViewerString(ImgViewerStringId::ActualSize), state_.initial_image_view_mode == InitialImageViewMode::ActualSize));
        view_.initial_fit_window_radio->SetOnSelected([this]() { SelectInitialFitWindow(); });
        view_.initial_actual_size_radio->SetOnSelected([this]() { SelectInitialActualSize(); });
        AddCheckboxSetting(image_section, kBooleanSettingSpecs[kPixelatedSamplingSetting], state_.pixelated_sampling, &view_.pixelated_sampling);
        AddCheckboxSetting(image_section, kBooleanSettingSpecs[kCheckerboardBackgroundSetting], state_.checkerboard_background, &view_.checkerboard_background);

        StackPanel* frame_section = AddSection(ImgViewerStringId::WindowFrame);
        AddCheckboxSetting(frame_section, kBooleanSettingSpecs[kBorderlessWindowSetting], state_.borderless_window, &view_.borderless_window);

        StackPanel* opacity_section = AddSection(ImgViewerStringId::Opacity, ui_theme::metrics::kSmallGap);
        AddSliderSetting(opacity_section, kSliderSettingSpecs[kOpacitySliderSetting], state_.window_opacity_percent, &view_.opacity);

        StackPanel* toolbar_section = AddSection(ImgViewerStringId::ToolbarSize, ui_theme::metrics::kSmallGap);
        AddSliderSetting(toolbar_section, kSliderSettingSpecs[kToolbarScaleSliderSetting], state_.toolbar_scale_percent, &view_.toolbar_scale);

        StackPanel* navigation_section = AddSection(ImgViewerStringId::Navigation, ui_theme::metrics::kSmallGap);
        AddCheckboxSetting(navigation_section, kBooleanSettingSpecs[kEdgeClickNavigationSetting], state_.edge_click_navigation, &view_.edge_click_navigation);
        AddSliderSetting(navigation_section, kSliderSettingSpecs[kEdgeClickZoneSliderSetting], state_.edge_click_navigation_zone_percent, &view_.edge_click_zone);

        StackPanel* filter_section = AddSection(ImgViewerStringId::ShortcutFilter, ui_theme::metrics::kSmallGap);
        view_.filter_box = filter_section->AddItem(std::make_unique<TextBox>(
            UiMetadata(UiElementRole::Edit, ImgViewerString(ImgViewerStringId::ShortcutFilter), kUiTooltipFromName),
            ImgViewerString(ImgViewerStringId::FilterActions)));

        StackPanel* shortcuts_section = AddSection(ImgViewerStringId::ActionShortcuts, 8.0f);
        view_.action_table = shortcuts_section->AddItem(std::make_unique<Table>(
            UiMetadata(UiElementRole::Pane, ImgViewerString(ImgViewerStringId::ActionShortcuts), kUiTooltipFromName)));
        view_.action_table->SetColumns(std::vector<TableColumn>{
            TableColumn{ImgViewerString(ImgViewerStringId::Action), 170.0f, false},
            TableColumn{ImgViewerString(ImgViewerStringId::Shortcut), 0.0f, true},
        });
        view_.action_table->SetHeaderVisible(true);
        view_.action_table->SetSelectionEnabled(true);
        view_.action_table->SetRowHeight(21.0f);

        footer_panel_ = root_->AddItem(ui_decl::HStack());
        footer_panel_->SetGap(kSettingsFooterButtonGap);
        footer_panel_->SetPadding(UiThickness{0.0f, ui_theme::metrics::kLargeGap, 0.0f, 0.0f});

        view_.reset_button = AddFooterButton(ImgViewerAction::ResetKeyBindings, ImgViewerString(ImgViewerStringId::ResetShortcuts), kResetIcon, ImgViewerString(ImgViewerStringId::Reset));
        view_.save_button = AddFooterButton(ImgViewerAction::SaveSettings, ImgViewerString(ImgViewerStringId::Save), kSaveIcon, ImgViewerString(ImgViewerStringId::Save));
        view_.cancel_button = AddFooterButton(ImgViewerAction::CloseSettings, ImgViewerString(ImgViewerStringId::Cancel), kCancelIcon, ImgViewerString(ImgViewerStringId::Cancel));
    }

    void SelectRememberWindowSize()
    {
        state_.remember_window_size.Set(true);
    }

    void SelectDefaultWindowSize()
    {
        state_.remember_window_size.Set(false);
    }

    void SelectInitialFitWindow()
    {
        state_.initial_image_view_mode = InitialImageViewMode::FitWindow;
        UpdateInitialImageViewModeControls();
    }

    void SelectInitialActualSize()
    {
        state_.initial_image_view_mode = InitialImageViewMode::ActualSize;
        UpdateInitialImageViewModeControls();
    }

    void SetSliderValue(SliderControl& control, util::Signal<int>& signal, int value)
    {
        const int clamped = control.spec->clamp(value);
        signal.Set(clamped);
        if (control.row != nullptr) {
            control.row->SetValue(clamped);
        }
        UpdateSliderText(control, clamped);
    }

    void UpdateSliderText(SliderControl& control, int value)
    {
        wchar_t text[16] = {};
        swprintf_s(text, L"%d%%", value);
        control.value_text = text;
        if (control.row != nullptr) {
            control.row->SetValueText(control.value_text.c_str());
        }
    }

    void UpdateInitialImageViewModeControls()
    {
        if (view_.initial_fit_window_radio != nullptr) {
            view_.initial_fit_window_radio->SetSelected(state_.initial_image_view_mode == InitialImageViewMode::FitWindow);
        }
        if (view_.initial_actual_size_radio != nullptr) {
            view_.initial_actual_size_radio->SetSelected(state_.initial_image_view_mode == InitialImageViewMode::ActualSize);
        }
    }

    void ConnectSignals()
    {
        state_.remember_window_size.Subscribe([this](bool value) {
            if (view_.remember_window_size.checkbox != nullptr) {
                view_.remember_window_size.checkbox->SetChecked(value);
            }
            if (view_.remember_radio != nullptr) {
                view_.remember_radio->SetSelected(value);
            }
            if (view_.default_radio != nullptr) {
                view_.default_radio->SetSelected(!value);
            }
        });
        state_.pixelated_sampling.Subscribe([this](bool value) {
            if (view_.pixelated_sampling.checkbox != nullptr) {
                view_.pixelated_sampling.checkbox->SetChecked(value);
            }
        });
        state_.checkerboard_background.Subscribe([this](bool value) {
            if (view_.checkerboard_background.checkbox != nullptr) {
                view_.checkerboard_background.checkbox->SetChecked(value);
            }
        });
        state_.borderless_window.Subscribe([this](bool value) {
            if (view_.borderless_window.checkbox != nullptr) {
                view_.borderless_window.checkbox->SetChecked(value);
            }
        });
        state_.edge_click_navigation.Subscribe([this](bool value) {
            if (view_.edge_click_navigation.checkbox != nullptr) {
                view_.edge_click_navigation.checkbox->SetChecked(value);
            }
        });

        state_.window_opacity_percent.Subscribe([this](int value) {
            const int clamped = ClampWindowOpacityPercent(value);
            if (view_.opacity.row != nullptr) {
                view_.opacity.row->SetValue(clamped);
            }
            UpdateSliderText(view_.opacity, clamped);
        });
        state_.toolbar_scale_percent.Subscribe([this](int value) {
            const int clamped = ClampToolbarScalePercent(value);
            if (view_.toolbar_scale.row != nullptr) {
                view_.toolbar_scale.row->SetValue(clamped);
            }
            UpdateSliderText(view_.toolbar_scale, clamped);
        });
        state_.edge_click_navigation_zone_percent.Subscribe([this](int value) {
            const int clamped = ClampEdgeClickNavigationZonePercent(value);
            if (view_.edge_click_zone.row != nullptr) {
                view_.edge_click_zone.row->SetValue(clamped);
            }
            UpdateSliderText(view_.edge_click_zone, clamped);
        });

        state_.language_index_signal.Subscribe([this](int) {
            if (view_.language_dropdown != nullptr) {
                view_.language_dropdown->SetSelectedIndex(static_cast<size_t>(state_.language_index_signal.Get()));
            }
        });
    }

    bool MatchesFilter(ImgViewerAction action) const
    {
        const std::wstring& filter = view_.filter_box->Text();
        if (filter.empty()) {
            return true;
        }
        std::wstring haystack = ImgViewerActionDisplayName(action);
        haystack += L" ";
        haystack += ShortcutsForAction(state_.action_bindings, action);
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
                    .cells = {ImgViewerString(action.display_name), ShortcutsForAction(state_.action_bindings, action.action)},
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
        view_.action_table->SetRows(BuildShortcutRows());
    }

    SettingsState state_;
    ViewRefs view_;
    std::unique_ptr<ScrollPanel> root_owner_;
    ScrollPanel* scroll_root_ = nullptr;
    StackPanel* root_ = nullptr;
    StackPanel* footer_panel_ = nullptr;
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
