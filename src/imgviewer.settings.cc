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

    static std::array<util::Signal<bool>, kBooleanSettingSpecs.size()> CreateBooleanSignals(const ImgViewerConfig& config)
    {
        return std::array<util::Signal<bool>, kBooleanSettingSpecs.size()>{{
            util::Signal<bool>(BooleanSettingValue(config, kBooleanSettingSpecs[0])),
            util::Signal<bool>(BooleanSettingValue(config, kBooleanSettingSpecs[1])),
            util::Signal<bool>(BooleanSettingValue(config, kBooleanSettingSpecs[2])),
            util::Signal<bool>(BooleanSettingValue(config, kBooleanSettingSpecs[3])),
            util::Signal<bool>(BooleanSettingValue(config, kBooleanSettingSpecs[4])),
        }};
    }

    static std::array<util::Signal<int>, kSliderSettingSpecs.size()> CreateSliderSignals(const ImgViewerConfig& config)
    {
        return std::array<util::Signal<int>, kSliderSettingSpecs.size()>{{
            util::Signal<int>(config.*(kSliderSettingSpecs[0].field)),
            util::Signal<int>(config.*(kSliderSettingSpecs[1].field)),
            util::Signal<int>(config.*(kSliderSettingSpecs[2].field)),
        }};
    }

public:
    explicit SettingsUi(ImgViewerConfig config) :
        action_bindings_(std::move(config.action_bindings)),
        initial_image_view_mode_(config.initial_image_view_mode),
        boolean_signals_(CreateBooleanSignals(config)),
        slider_signals_(CreateSliderSignals(config)),
        language_index_signal_(static_cast<int>(LanguageIndex(config.language)))
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
        config.language = LanguageFromIndex(static_cast<size_t>(language_index_signal_.Get()));
        config.initial_image_view_mode = initial_image_view_mode_;
        for (size_t index = 0; index < boolean_signals_.size(); ++index) {
            const BooleanSettingSpec& spec = kBooleanSettingSpecs[index];
            config.*(spec.field) = spec.inverted ? !boolean_signals_[index].Get() : boolean_signals_[index].Get();
        }
        for (size_t index = 0; index < slider_signals_.size(); ++index) {
            const SliderSettingSpec& spec = kSliderSettingSpecs[index];
            config.*(spec.field) = spec.clamp(slider_signals_[index].Get());
        }
        config.action_bindings = action_bindings_;
        return config;
    }
    int OpacityPercent() const { return slider_signals_[kOpacitySliderSetting].Get(); }
    int ToolbarScalePercent() const { return slider_signals_[kToolbarScaleSliderSetting].Get(); }

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
    StackPanel* AddSection(ImgViewerStringId label, float gap = ui_theme::metrics::kStandardGap)
    {
        auto content = ui_decl::VStack();
        content->SetGap(gap);
        StackPanel* section = content.get();
        root_->AddItem(ui_decl::Section(ImgViewerString(label), std::move(content)));
        return section;
    }

    Checkbox* AddCheckboxSetting(StackPanel* section, size_t index)
    {
        const BooleanSettingSpec& spec = kBooleanSettingSpecs[index];
        auto checkbox = ui_decl::Toggle(
            ImgViewerString(spec.label),
            boolean_signals_[index].Get());
        Checkbox* result = section->AddItem(std::move(checkbox));
        boolean_controls_[index] = BooleanControl{.spec = &spec, .checkbox = result};
        result->SetOnToggled([this, index](bool checked) {
            boolean_signals_[index].Set(checked);
        });
        return result;
    }

    SliderRow* AddSliderSetting(StackPanel* section, size_t index)
    {
        const SliderSettingSpec& spec = kSliderSettingSpecs[index];
        auto slider = ui_decl::SliderField(
            ImgViewerString(spec.label),
            spec.minimum,
            spec.maximum,
            slider_signals_[index].Get(),
            spec.small_step,
            spec.large_step);
        SliderRow* result = section->AddItem(std::move(slider));
        slider_controls_[index] = SliderControl{.spec = &spec, .row = result};
        result->GetSlider()->SetOnValueChanged([this, index](int value) {
            SetSliderValue(index, value);
        });
        result->GetSlider()->SetAccessibilityValueChangedHandler([this, index](int value) {
            SetSliderValue(index, value);
        });
        UpdateSliderText(slider_controls_[index], slider_signals_[index].Get());
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
        dropdown->SetSelectedIndex(static_cast<size_t>(language_index_signal_.Get()));
        Dropdown* result = section->AddItem(std::move(dropdown));
        result->SetOnSelectionChanged([this](size_t index) {
            language_index_signal_.Set(static_cast<int>(index));
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
        language_dropdown_ = AddLanguageDropdown(language_section);

        StackPanel* window_size_section = AddSection(ImgViewerStringId::WindowSize, 3.0f);
        AddCheckboxSetting(window_size_section, kRememberWindowSizeSetting);
        remember_radio_ = window_size_section->AddItem(
            ui_decl::Radio(ImgViewerString(ImgViewerStringId::RememberLastSize), boolean_signals_[kRememberWindowSizeSetting].Get()));
        default_radio_ = window_size_section->AddItem(
            ui_decl::Radio(ImgViewerString(ImgViewerStringId::UseDefaultSize), !boolean_signals_[kRememberWindowSizeSetting].Get()));
        remember_radio_->SetOnSelected([this]() { SelectRememberWindowSize(); });
        default_radio_->SetOnSelected([this]() { SelectDefaultWindowSize(); });

        StackPanel* image_section = AddSection(ImgViewerStringId::ImageRendering);
        image_section->AddItem(ui_decl::Muted(ImgViewerString(ImgViewerStringId::InitialImageView)));
        initial_fit_window_radio_ = image_section->AddItem(
            ui_decl::Radio(ImgViewerString(ImgViewerStringId::FitWindow), initial_image_view_mode_ == InitialImageViewMode::FitWindow));
        initial_actual_size_radio_ = image_section->AddItem(
            ui_decl::Radio(ImgViewerString(ImgViewerStringId::ActualSize), initial_image_view_mode_ == InitialImageViewMode::ActualSize));
        initial_fit_window_radio_->SetOnSelected([this]() { SelectInitialFitWindow(); });
        initial_actual_size_radio_->SetOnSelected([this]() { SelectInitialActualSize(); });
        AddCheckboxSetting(image_section, kPixelatedSamplingSetting);
        AddCheckboxSetting(image_section, kCheckerboardBackgroundSetting);

        StackPanel* frame_section = AddSection(ImgViewerStringId::WindowFrame);
        AddCheckboxSetting(frame_section, kBorderlessWindowSetting);

        StackPanel* opacity_section = AddSection(ImgViewerStringId::Opacity, ui_theme::metrics::kSmallGap);
        AddSliderSetting(opacity_section, kOpacitySliderSetting);

        StackPanel* toolbar_section = AddSection(ImgViewerStringId::ToolbarSize, ui_theme::metrics::kSmallGap);
        AddSliderSetting(toolbar_section, kToolbarScaleSliderSetting);

        StackPanel* navigation_section = AddSection(ImgViewerStringId::Navigation, ui_theme::metrics::kSmallGap);
        AddCheckboxSetting(navigation_section, kEdgeClickNavigationSetting);
        AddSliderSetting(navigation_section, kEdgeClickZoneSliderSetting);

        StackPanel* filter_section = AddSection(ImgViewerStringId::ShortcutFilter, ui_theme::metrics::kSmallGap);
        filter_box_ = filter_section->AddItem(std::make_unique<TextBox>(
            UiMetadata(UiElementRole::Edit, ImgViewerString(ImgViewerStringId::ShortcutFilter), kUiTooltipFromName),
            ImgViewerString(ImgViewerStringId::FilterActions)));

        StackPanel* shortcuts_section = AddSection(ImgViewerStringId::ActionShortcuts, 8.0f);
        action_table_ = shortcuts_section->AddItem(std::make_unique<Table>(
            UiMetadata(UiElementRole::Pane, ImgViewerString(ImgViewerStringId::ActionShortcuts), kUiTooltipFromName)));
        action_table_->SetColumns(std::vector<TableColumn>{
            TableColumn{ImgViewerString(ImgViewerStringId::Action), 170.0f, false},
            TableColumn{ImgViewerString(ImgViewerStringId::Shortcut), 0.0f, true},
        });
        action_table_->SetHeaderVisible(true);
        action_table_->SetSelectionEnabled(true);
        action_table_->SetRowHeight(21.0f);

        footer_panel_ = root_->AddItem(ui_decl::HStack());
        footer_panel_->SetGap(kSettingsFooterButtonGap);
        footer_panel_->SetPadding(UiThickness{0.0f, ui_theme::metrics::kLargeGap, 0.0f, 0.0f});

        reset_button_ = AddFooterButton(ImgViewerAction::ResetKeyBindings, ImgViewerString(ImgViewerStringId::ResetShortcuts), kResetIcon, ImgViewerString(ImgViewerStringId::Reset));
        save_button_ = AddFooterButton(ImgViewerAction::SaveSettings, ImgViewerString(ImgViewerStringId::Save), kSaveIcon, ImgViewerString(ImgViewerStringId::Save));
        cancel_button_ = AddFooterButton(ImgViewerAction::CloseSettings, ImgViewerString(ImgViewerStringId::Cancel), kCancelIcon, ImgViewerString(ImgViewerStringId::Cancel));
    }

    void SelectRememberWindowSize()
    {
        boolean_signals_[kRememberWindowSizeSetting].Set(true);
    }

    void SelectDefaultWindowSize()
    {
        boolean_signals_[kRememberWindowSizeSetting].Set(false);
    }

    void SelectInitialFitWindow()
    {
        initial_image_view_mode_ = InitialImageViewMode::FitWindow;
        UpdateInitialImageViewModeControls();
    }

    void SelectInitialActualSize()
    {
        initial_image_view_mode_ = InitialImageViewMode::ActualSize;
        UpdateInitialImageViewModeControls();
    }

    void SetSliderValue(size_t index, int value)
    {
        SetSliderValue(slider_controls_[index], value);
    }

    void SetSliderValue(SliderControl& control, int value)
    {
        const int clamped = control.spec->clamp(value);
        const size_t index = static_cast<size_t>(&control - slider_controls_.data());
        slider_signals_[index].Set(clamped);
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
        if (initial_fit_window_radio_ != nullptr) {
            initial_fit_window_radio_->SetSelected(initial_image_view_mode_ == InitialImageViewMode::FitWindow);
        }
        if (initial_actual_size_radio_ != nullptr) {
            initial_actual_size_radio_->SetSelected(initial_image_view_mode_ == InitialImageViewMode::ActualSize);
        }
    }

    void ConnectSignals()
    {
        for (size_t index = 0; index < boolean_signals_.size(); ++index) {
            boolean_signals_[index].Subscribe([this, index](bool) {
                BooleanControl& control = boolean_controls_[index];
                if (control.checkbox != nullptr) {
                    control.checkbox->SetChecked(boolean_signals_[index].Get());
                }
                if (index == kRememberWindowSizeSetting) {
                    if (remember_radio_ != nullptr) {
                        remember_radio_->SetSelected(boolean_signals_[index].Get());
                    }
                    if (default_radio_ != nullptr) {
                        default_radio_->SetSelected(!boolean_signals_[index].Get());
                    }
                }
            });
        }

        for (size_t index = 0; index < slider_signals_.size(); ++index) {
            slider_signals_[index].Subscribe([this, index](int value) {
                SliderControl& control = slider_controls_[index];
                const SliderSettingSpec& spec = kSliderSettingSpecs[index];
                const int clamped = spec.clamp(value);
                if (control.row != nullptr) {
                    control.row->SetValue(clamped);
                }
                UpdateSliderText(control, clamped);
            });
        }

        language_index_signal_.Subscribe([this](int) {
            if (language_dropdown_ != nullptr) {
                language_dropdown_->SetSelectedIndex(static_cast<size_t>(language_index_signal_.Get()));
            }
        });
    }

    bool MatchesFilter(ImgViewerAction action) const
    {
        const std::wstring& filter = filter_box_->Text();
        if (filter.empty()) {
            return true;
        }
        std::wstring haystack = ImgViewerActionDisplayName(action);
        haystack += L" ";
        haystack += ShortcutsForAction(action_bindings_, action);
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
                    .cells = {ImgViewerString(action.display_name), ShortcutsForAction(action_bindings_, action.action)},
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

    ActionBindings action_bindings_;
    InitialImageViewMode initial_image_view_mode_ = InitialImageViewMode::FitWindow;
    std::array<util::Signal<bool>, kBooleanSettingSpecs.size()> boolean_signals_;
    std::array<util::Signal<int>, kSliderSettingSpecs.size()> slider_signals_;
    util::Signal<int> language_index_signal_;
    std::unique_ptr<ScrollPanel> root_owner_;
    ScrollPanel* scroll_root_ = nullptr;
    StackPanel* root_ = nullptr;
    StackPanel* footer_panel_ = nullptr;
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
