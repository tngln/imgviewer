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
#include "experimental/ui.bind.hpp"
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
namespace ui_bind = experimental::ui_bind;

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
constexpr float kSettingsFooterButtonWidth = 72.0f;
constexpr float kSettingsFooterButtonGap = 5.0f;

namespace settings_detail {

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
    util::Signal<int> initial_image_view_mode_index;
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
        util::Signal<int> image_view_mode_index,
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
        initial_image_view_mode_index(std::move(image_view_mode_index)),
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

struct ViewRefs final {
    TextBox* filter_box = nullptr;
    Table* action_table = nullptr;
};

inline size_t LanguageIndex(ImgViewerLanguage language)
{
    return language == ImgViewerLanguage::SimplifiedChinese ? 1 : 0;
}

inline int InitialImageViewModeIndex(InitialImageViewMode mode)
{
    return mode == InitialImageViewMode::ActualSize ? 1 : 0;
}

inline InitialImageViewMode InitialImageViewModeFromIndex(int index)
{
    return index == 1 ? InitialImageViewMode::ActualSize : InitialImageViewMode::FitWindow;
}

inline ImgViewerLanguage LanguageFromIndex(size_t index)
{
    return index == 1 ? ImgViewerLanguage::SimplifiedChinese : ImgViewerLanguage::English;
}

inline SettingsState CreateState(ImgViewerConfig config)
{
    return SettingsState(
        std::move(config.action_bindings),
        util::Signal<int>(InitialImageViewModeIndex(config.initial_image_view_mode)),
        util::Signal<bool>(config.remember_window_size),
        util::Signal<bool>(config.pixelated_sampling),
        util::Signal<bool>(config.checkerboard_background),
        util::Signal<bool>(config.borderless_window),
        util::Signal<bool>(config.edge_click_navigation),
        util::Signal<int>(config.window_opacity_percent),
        util::Signal<int>(config.toolbar_scale_percent),
        util::Signal<int>(config.edge_click_navigation_zone_percent),
        util::Signal<int>(static_cast<int>(LanguageIndex(config.language))));
}

} // namespace settings_detail


class SettingsUi final : public UiRoot {
private:
public:
    explicit SettingsUi(ImgViewerConfig config) :
        state_(settings_detail::CreateState(std::move(config)))
    {
        auto scroll_panel = std::make_unique<ScrollPanel>(
            UiMetadata(UiElementRole::Pane, ImgViewerString(ImgViewerStringId::Settings), kUiTooltipFromName, false, true));
        scroll_panel->SetScrollStep(42.0f);
        auto root_panel = std::make_unique<StackPanel>(
            UiRootMetadata(UiElementRole::Pane, ImgViewerString(ImgViewerStringId::Settings), kUiTooltipFromName));
        root_panel->SetPadding(UiThickness{kSettingsSidePadding, kSettingsContentTopPadding, kSettingsSidePadding, kSettingsFooterBottomPadding});
        root_panel->SetGap(0.0f);
        root_ = root_panel.get();
        scroll_panel->SetContent(std::move(root_panel));
        root_owner_ = std::move(scroll_panel);

        BuildUiTree();
        UpdateFilterResults();
    }

    ImgViewerConfig Draft() const
    {
        ImgViewerConfig config;
        config.language = settings_detail::LanguageFromIndex(static_cast<size_t>(state_.language_index_signal.Get()));
        config.initial_image_view_mode = settings_detail::InitialImageViewModeFromIndex(state_.initial_image_view_mode_index.Get());
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
        state_.window_opacity_percent.Set(ClampWindowOpacityPercent(percent));
    }

    void SetToolbarScalePercent(int percent)
    {
        state_.toolbar_scale_percent.Set(ClampToolbarScalePercent(percent));
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
    void BuildUiTree()
    {
        const auto bound = ui_decl::Bind(bindings_);
        root_->AddItem(ui_decl::Title(ImgViewerString(ImgViewerStringId::Settings)), 17.0f);

        {
            auto content = ui_decl::VStack().Gap(ui_theme::metrics::kSmallGap);
            StackPanel& section = *content.Get();
            section.AddItem(
                bound.DropdownField(
                    ImgViewerString(ImgViewerStringId::Language),
                    std::vector<DropdownOption>{
                        DropdownOption{ImgViewerString(ImgViewerStringId::EnglishLanguage), kUiActionNone},
                        DropdownOption{ImgViewerString(ImgViewerStringId::SimplifiedChineseLanguage), kUiActionNone},
                    },
                    state_.language_index_signal));
            root_->AddItem(ui_decl::Section(ImgViewerString(ImgViewerStringId::Language), std::move(content)));
        }

        {
            auto content = ui_decl::VStack().Gap(3.0f);
            StackPanel& section = *content.Get();
            section.AddItem(bound.Toggle(ImgViewerString(ImgViewerStringId::RememberWindowSize), state_.remember_window_size));
            section.AddItem(
                bound.Radio(ImgViewerString(ImgViewerStringId::RememberLastSize), state_.remember_window_size, true));
            section.AddItem(
                bound.Radio(ImgViewerString(ImgViewerStringId::UseDefaultSize), state_.remember_window_size, false));
            root_->AddItem(ui_decl::Section(ImgViewerString(ImgViewerStringId::WindowSize), std::move(content)));
        }

        {
            auto content = ui_decl::VStack().Gap(ui_theme::metrics::kStandardGap);
            StackPanel& section = *content.Get();
            section.AddItem(ui_decl::Muted(ImgViewerString(ImgViewerStringId::InitialImageView)));
            section.AddItem(
                bound.Radio(ImgViewerString(ImgViewerStringId::FitWindow), state_.initial_image_view_mode_index, 0));
            section.AddItem(
                bound.Radio(ImgViewerString(ImgViewerStringId::ActualSize), state_.initial_image_view_mode_index, 1));
            section.AddItem(bound.Toggle(ImgViewerString(ImgViewerStringId::PixelatedSampling), state_.pixelated_sampling));
            section.AddItem(bound.Toggle(ImgViewerString(ImgViewerStringId::CheckerboardBackground), state_.checkerboard_background));
            root_->AddItem(ui_decl::Section(ImgViewerString(ImgViewerStringId::ImageRendering), std::move(content)));
        }

        {
            auto content = ui_decl::VStack().Gap(ui_theme::metrics::kStandardGap);
            StackPanel& section = *content.Get();
            section.AddItem(bound.Toggle(ImgViewerString(ImgViewerStringId::BorderlessWindow), state_.borderless_window));
            root_->AddItem(ui_decl::Section(ImgViewerString(ImgViewerStringId::WindowFrame), std::move(content)));
        }

        {
            auto content = ui_decl::VStack().Gap(ui_theme::metrics::kSmallGap);
            StackPanel& section = *content.Get();
            section.AddItem(
                bound.SliderField(
                    ImgViewerString(ImgViewerStringId::Opacity),
                    kOpacityMinimum,
                    kOpacityMaximum,
                    state_.window_opacity_percent,
                    kOpacitySmallStep,
                    kOpacityLargeStep,
                    [this](int value) { return ClampWindowOpacityPercent(value); },
                    [](int value) {
                        wchar_t text[16] = {};
                        swprintf_s(text, L"%d%%", value);
                        return std::wstring(text);
                    }));
            root_->AddItem(ui_decl::Section(ImgViewerString(ImgViewerStringId::Opacity), std::move(content)));
        }

        {
            auto content = ui_decl::VStack().Gap(ui_theme::metrics::kSmallGap);
            StackPanel& section = *content.Get();
            section.AddItem(
                bound.SliderField(
                    ImgViewerString(ImgViewerStringId::ToolbarSize),
                    kToolbarScaleMinimum,
                    kToolbarScaleMaximum,
                    state_.toolbar_scale_percent,
                    kToolbarScaleSmallStep,
                    kToolbarScaleLargeStep,
                    [this](int value) { return ClampToolbarScalePercent(value); },
                    [](int value) {
                        wchar_t text[16] = {};
                        swprintf_s(text, L"%d%%", value);
                        return std::wstring(text);
                    }));
            root_->AddItem(ui_decl::Section(ImgViewerString(ImgViewerStringId::ToolbarSize), std::move(content)));
        }

        {
            auto content = ui_decl::VStack().Gap(ui_theme::metrics::kSmallGap);
            StackPanel& section = *content.Get();
            section.AddItem(bound.Toggle(ImgViewerString(ImgViewerStringId::EdgeClickNavigation), state_.edge_click_navigation));
            section.AddItem(
                bound.SliderField(
                    ImgViewerString(ImgViewerStringId::EdgeClickZone),
                    kEdgeClickZoneMinimum,
                    kEdgeClickZoneMaximum,
                    state_.edge_click_navigation_zone_percent,
                    kEdgeClickZoneSmallStep,
                    kEdgeClickZoneLargeStep,
                    [this](int value) { return ClampEdgeClickNavigationZonePercent(value); },
                    [](int value) {
                        wchar_t text[16] = {};
                        swprintf_s(text, L"%d%%", value);
                        return std::wstring(text);
                    }));
            root_->AddItem(ui_decl::Section(ImgViewerString(ImgViewerStringId::Navigation), std::move(content)));
        }

        {
            auto content = ui_decl::VStack().Gap(ui_theme::metrics::kSmallGap);
            StackPanel& section = *content.Get();
            view_.filter_box = section.AddItem(std::make_unique<TextBox>(
                UiMetadata(UiElementRole::Edit, ImgViewerString(ImgViewerStringId::ShortcutFilter), kUiTooltipFromName),
                ImgViewerString(ImgViewerStringId::FilterActions)));
            root_->AddItem(ui_decl::Section(ImgViewerString(ImgViewerStringId::ShortcutFilter), std::move(content)));
        }

        {
            auto content = ui_decl::VStack().Gap(8.0f);
            StackPanel& section = *content.Get();
            view_.action_table = section.AddItem(std::make_unique<Table>(
                UiMetadata(UiElementRole::Pane, ImgViewerString(ImgViewerStringId::ActionShortcuts), kUiTooltipFromName)));
            view_.action_table->SetColumns(std::vector<TableColumn>{
                TableColumn{ImgViewerString(ImgViewerStringId::Action), 170.0f, false},
                TableColumn{ImgViewerString(ImgViewerStringId::Shortcut), 0.0f, true},
            });
            view_.action_table->SetHeaderVisible(true);
            view_.action_table->SetSelectionEnabled(true);
            view_.action_table->SetRowHeight(21.0f);
            root_->AddItem(ui_decl::Section(ImgViewerString(ImgViewerStringId::ActionShortcuts), std::move(content)));
        }

        StackPanel* footer_panel = root_->AddItem(
            ui_decl::HStack()
                .Gap(kSettingsFooterButtonGap)
                .Padding(UiThickness{0.0f, ui_theme::metrics::kLargeGap, 0.0f, 0.0f}));

        footer_panel->AddItem(
            ui_decl::ActionButton(
                UiActionFromImgViewerAction(ImgViewerAction::ResetKeyBindings),
                ImgViewerString(ImgViewerStringId::ResetShortcuts),
                kResetIcon,
                ImgViewerString(ImgViewerStringId::Reset)),
            kSettingsFooterButtonWidth);
        footer_panel->AddItem(
            ui_decl::ActionButton(
                UiActionFromImgViewerAction(ImgViewerAction::SaveSettings),
                ImgViewerString(ImgViewerStringId::Save),
                kSaveIcon,
                ImgViewerString(ImgViewerStringId::Save)),
            kSettingsFooterButtonWidth);
        footer_panel->AddItem(
            ui_decl::ActionButton(
                UiActionFromImgViewerAction(ImgViewerAction::CloseSettings),
                ImgViewerString(ImgViewerStringId::Cancel),
                kCancelIcon,
                ImgViewerString(ImgViewerStringId::Cancel)),
            kSettingsFooterButtonWidth);
    }

    int ClampSliderValue(const util::Signal<int>& signal, int value) const
    {
        if (&signal == &state_.window_opacity_percent) {
            return ClampWindowOpacityPercent(value);
        }
        if (&signal == &state_.toolbar_scale_percent) {
            return ClampToolbarScalePercent(value);
        }
        return ClampEdgeClickNavigationZonePercent(value);
    }

    bool MatchesFilter(ImgViewerAction action) const
    {
        const std::wstring& filter = view_.filter_box->Text();
        if (filter.empty()) {
            return true;
        }
        std::wstring haystack = ImgViewerActionDisplayName(action);
        haystack += L" ";
        haystack += settings_detail::ShortcutsForAction(state_.action_bindings, action);
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
                    .cells = {ImgViewerString(action.display_name), settings_detail::ShortcutsForAction(state_.action_bindings, action.action)},
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

    settings_detail::SettingsState state_;
    settings_detail::ViewRefs view_;
    ui_bind::SubscriptionBag bindings_;
    std::unique_ptr<ScrollPanel> root_owner_;
    StackPanel* root_ = nullptr;
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
                .title = ImgViewerString(ImgViewerStringId::Settings),
                .frame = win32::NativeWindowFrame::Dialog,
                .width = kSettingsInitialWidth,
                .height = kSettingsInitialHeight,
                .owner = owner,
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
