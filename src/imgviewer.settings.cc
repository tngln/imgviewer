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
#include "imgviewer.ui.action.hpp"
#include "ui.button.hpp"
#include "ui.draw.hpp"
#include "ui.layout.hpp"
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
constexpr int kToolbarScaleMinimum = 80;
constexpr int kToolbarScaleMaximum = 160;
constexpr int kToolbarScaleSmallStep = 5;
constexpr int kToolbarScaleLargeStep = 10;
constexpr int kSettingsInitialWidth = 820;
constexpr int kSettingsInitialHeight = 944;
constexpr int kSettingsMinClientWidth = 620;
constexpr int kSettingsMinClientHeight = 916;

constexpr float kSettingsSidePadding = 28.0f;
constexpr float kSettingsFooterBottomPadding = 20.0f;
constexpr float kSettingsFooterButtonHeight = 48.0f;
constexpr float kSettingsFooterButtonGap = 10.0f;
constexpr float kSettingsResetButtonWidth = 138.0f;
constexpr float kSettingsSaveButtonWidth = 126.0f;
constexpr float kSettingsCancelButtonWidth = 128.0f;
constexpr float kSettingsLabelHeight = 26.0f;
constexpr float kSettingsChoiceHeight = 36.0f;
constexpr float kSettingsFieldHeight = 42.0f;
constexpr float kSettingsValueWidth = 72.0f;

struct SettingsLayoutRects final {
    D2D1_RECT_F title = {};
    D2D1_RECT_F window_size_label = {};
    D2D1_RECT_F image_rendering_label = {};
    D2D1_RECT_F window_frame_label = {};
    D2D1_RECT_F opacity_label = {};
    D2D1_RECT_F opacity_value = {};
    D2D1_RECT_F toolbar_scale_label = {};
    D2D1_RECT_F toolbar_scale_value = {};
    D2D1_RECT_F shortcut_filter_label = {};
    D2D1_RECT_F action_shortcuts_label = {};
    D2D1_RECT_F shortcut_text = {};
    D2D1_RECT_F remember_checkbox = {};
    D2D1_RECT_F remember_radio = {};
    D2D1_RECT_F default_radio = {};
    D2D1_RECT_F pixelated_checkbox = {};
    D2D1_RECT_F checkerboard_checkbox = {};
    D2D1_RECT_F borderless_checkbox = {};
    D2D1_RECT_F opacity_slider = {};
    D2D1_RECT_F toolbar_scale_slider = {};
    D2D1_RECT_F filter_box = {};
    D2D1_RECT_F action_dropdown = {};
    D2D1_RECT_F reset_button = {};
    D2D1_RECT_F save_button = {};
    D2D1_RECT_F cancel_button = {};
};

D2D1_RECT_F FullWidthRect(float left, float top, float right, float height)
{
    return D2D1::RectF(left, top, right, top + height);
}

D2D1_RECT_F BelowFullWidth(D2D1_RECT_F anchor, float gap, float left, float right, float height)
{
    const D2D1_RECT_F rect = ui_layout::Below(anchor, gap, right - left, height);
    return D2D1::RectF(left, rect.top, right, rect.bottom);
}

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

SettingsLayoutRects CalculateSettingsLayout(D2D1_SIZE_F size)
{
    SettingsLayoutRects layout;
    const float left = kSettingsSidePadding;
    const float right = size.width - kSettingsSidePadding;
    const float radio_left = left + 24.0f;
    const float value_left = size.width - kSettingsSidePadding - kSettingsValueWidth;
    const D2D1_RECT_F root = D2D1::RectF(0.0f, 0.0f, size.width, size.height);

    layout.title = FullWidthRect(left, 18.0f, right, 34.0f);
    layout.remember_checkbox = BelowFullWidth(layout.title, 8.0f, left, right, kSettingsChoiceHeight);
    layout.window_size_label = BelowFullWidth(layout.remember_checkbox, 8.0f, left, right, kSettingsLabelHeight);

    const std::vector<D2D1_RECT_F> size_choices = ui_layout::PlaceVerticalStack(
        D2D1::RectF(radio_left, layout.window_size_label.bottom + 10.0f, right, 0.0f),
        std::vector<float>{kSettingsChoiceHeight, kSettingsChoiceHeight});
    layout.remember_radio = size_choices[0];
    layout.default_radio = size_choices[1];

    layout.image_rendering_label = BelowFullWidth(layout.default_radio, 16.0f, left, right, kSettingsLabelHeight);
    const std::vector<D2D1_RECT_F> rendering_choices = ui_layout::PlaceVerticalStack(
        D2D1::RectF(left, layout.image_rendering_label.bottom + 6.0f, right, 0.0f),
        std::vector<float>{kSettingsChoiceHeight, kSettingsChoiceHeight});
    layout.pixelated_checkbox = rendering_choices[0];
    layout.checkerboard_checkbox = rendering_choices[1];

    layout.window_frame_label = BelowFullWidth(layout.checkerboard_checkbox, 24.0f, left, right, kSettingsLabelHeight);
    layout.borderless_checkbox = BelowFullWidth(layout.window_frame_label, 6.0f, left, right, kSettingsChoiceHeight);

    layout.opacity_label = BelowFullWidth(layout.borderless_checkbox, 16.0f, left, right, kSettingsLabelHeight);
    layout.opacity_slider = BelowFullWidth(layout.opacity_label, 8.0f, left, value_left - 16.0f, kSettingsChoiceHeight);
    layout.opacity_value = D2D1::RectF(value_left, layout.opacity_slider.top - 4.0f, right, layout.opacity_slider.bottom - 4.0f);

    layout.toolbar_scale_label = BelowFullWidth(layout.opacity_slider, 16.0f, left, right, kSettingsLabelHeight);
    layout.toolbar_scale_slider =
        BelowFullWidth(layout.toolbar_scale_label, 8.0f, left, value_left - 16.0f, kSettingsChoiceHeight);
    layout.toolbar_scale_value =
        D2D1::RectF(value_left, layout.toolbar_scale_slider.top - 4.0f, right, layout.toolbar_scale_slider.bottom - 4.0f);

    layout.shortcut_filter_label =
        BelowFullWidth(layout.toolbar_scale_slider, 16.0f, left, right, kSettingsLabelHeight);
    layout.filter_box = BelowFullWidth(layout.shortcut_filter_label, 8.0f, left, right, kSettingsFieldHeight);
    layout.action_shortcuts_label = BelowFullWidth(layout.filter_box, 24.0f, left, right, kSettingsLabelHeight);
    layout.action_dropdown = BelowFullWidth(layout.action_shortcuts_label, 8.0f, left, right, kSettingsFieldHeight);
    layout.shortcut_text = BelowFullWidth(layout.action_dropdown, 18.0f, left, right, 30.0f);

    layout.reset_button = D2D1::RectF(
        left,
        size.height - kSettingsFooterBottomPadding - kSettingsFooterButtonHeight,
        left + kSettingsResetButtonWidth,
        size.height - kSettingsFooterBottomPadding);
    const std::vector<D2D1_RECT_F> primary_buttons = ui_layout::PlaceBottomRightRow(
        root,
        std::vector<float>{kSettingsSaveButtonWidth, kSettingsCancelButtonWidth},
        kSettingsFooterButtonHeight,
        kSettingsSidePadding - 8.0f,
        kSettingsFooterBottomPadding,
        kSettingsFooterButtonGap);
    layout.save_button = primary_buttons[0];
    layout.cancel_button = primary_buttons[1];
    return layout;
}

class SettingsUi final : public UiRoot {
public:
    explicit SettingsUi(ImgViewerConfig config) : draft_(std::move(config))
    {
        root_ = std::make_unique<UiElement>(
            UiRootMetadata(
                UiElementRole::Pane,
                UiActionFromImgViewerAction(ImgViewerAction::None),
                L"Settings",
                L"Settings",
                L"settings-root"));

        remember_checkbox_ = AddControl(std::make_unique<Checkbox>(
            UiMetadata(
                UiElementRole::CheckBox,
                UiActionFromImgViewerAction(ImgViewerAction::None),
                L"Remember window size",
                L"Remember window size",
                L"remember-window-size"),
            L"Remember window size",
            draft_.remember_window_size));
        remember_radio_ = AddControl(std::make_unique<RadioButton>(
            UiMetadata(
                UiElementRole::RadioButton,
                UiActionFromImgViewerAction(ImgViewerAction::None),
                L"Remember last size",
                L"Remember last size",
                L"remember-last-size"),
            L"Remember last size",
            draft_.remember_window_size));
        default_radio_ = AddControl(std::make_unique<RadioButton>(
            UiMetadata(
                UiElementRole::RadioButton,
                UiActionFromImgViewerAction(ImgViewerAction::None),
                L"Use default size",
                L"Use default size",
                L"use-default-size"),
            L"Use default size",
            !draft_.remember_window_size));
        pixelated_checkbox_ = AddControl(std::make_unique<Checkbox>(
            UiMetadata(
                UiElementRole::CheckBox,
                UiActionFromImgViewerAction(ImgViewerAction::None),
                L"Pixelated sampling",
                L"Pixelated sampling",
                L"pixelated-sampling"),
            L"Pixelated sampling",
            draft_.pixelated_sampling));
        checkerboard_checkbox_ = AddControl(std::make_unique<Checkbox>(
            UiMetadata(
                UiElementRole::CheckBox,
                UiActionFromImgViewerAction(ImgViewerAction::None),
                L"Checkerboard background",
                L"Checkerboard background",
                L"checkerboard-background"),
            L"Checkerboard background",
            draft_.checkerboard_background));
        borderless_checkbox_ = AddControl(std::make_unique<Checkbox>(
            UiMetadata(
                UiElementRole::CheckBox,
                UiActionFromImgViewerAction(ImgViewerAction::None),
                L"Borderless window",
                L"Borderless window",
                L"borderless-window"),
            L"Borderless window",
            draft_.borderless_window));
        opacity_slider_ = AddControl(std::make_unique<Slider>(
            UiMetadata(
                UiElementRole::Slider,
                UiActionFromImgViewerAction(ImgViewerAction::None),
                L"Opacity",
                L"Opacity",
                L"window-opacity"),
            kOpacityMinimum,
            kOpacityMaximum,
            draft_.window_opacity_percent,
            kOpacitySmallStep,
            kOpacityLargeStep));
        toolbar_scale_slider_ = AddControl(std::make_unique<Slider>(
            UiMetadata(
                UiElementRole::Slider,
                UiActionFromImgViewerAction(ImgViewerAction::None),
                L"Toolbar size",
                L"Toolbar size",
                L"toolbar-size"),
            kToolbarScaleMinimum,
            kToolbarScaleMaximum,
            draft_.toolbar_scale_percent,
            kToolbarScaleSmallStep,
            kToolbarScaleLargeStep));

        filter_box_ = AddControl(std::make_unique<TextBox>(
            UiMetadata(
                UiElementRole::Edit,
                UiActionFromImgViewerAction(ImgViewerAction::None),
                L"Shortcut filter",
                L"Shortcut filter",
                L"shortcut-filter"),
            L"Filter actions"));
        action_dropdown_ = AddControl(std::make_unique<Dropdown>(
            UiMetadata(
                UiElementRole::ComboBox,
                UiActionFromImgViewerAction(ImgViewerAction::None),
                L"Action shortcuts",
                L"Action shortcuts",
                L"action-shortcuts"),
            BuildDropdownOptions()));

        reset_button_ = AddControl(std::make_unique<Button>(
            UiMetadata(
                UiElementRole::Button,
                UiActionFromImgViewerAction(ImgViewerAction::ResetKeyBindings),
                L"Reset Shortcuts",
                L"Reset Shortcuts",
                L"reset-shortcuts"),
            kResetIcon,
            L"Reset"));
        save_button_ = AddControl(std::make_unique<Button>(
            UiMetadata(
                UiElementRole::Button,
                UiActionFromImgViewerAction(ImgViewerAction::SaveSettings),
                L"Save",
                L"Save",
                L"save-settings"),
            kSaveIcon,
            L"Save"));
        cancel_button_ = AddControl(std::make_unique<Button>(
            UiMetadata(
                UiElementRole::Button,
                UiActionFromImgViewerAction(ImgViewerAction::CloseSettings),
                L"Cancel",
                L"Cancel",
                L"cancel-settings"),
            kCancelIcon,
            L"Cancel"));

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
        opacity_slider_->SetValue(draft_.window_opacity_percent);
        UpdateOpacityText();
    }

    void SetToolbarScalePercent(int percent)
    {
        draft_.toolbar_scale_percent = ClampToolbarScalePercent(percent);
        toolbar_scale_slider_->SetValue(draft_.toolbar_scale_percent);
        UpdateToolbarScaleText();
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
        if (id == toolbar_scale_slider_->Id()) {
            return toolbar_scale_text_.c_str();
        }
        return L"";
    }

    double ElementRangeValue(UiElementId id) const override
    {
        if (id == opacity_slider_->Id()) {
            return static_cast<double>(opacity_slider_->Value());
        }
        if (id == toolbar_scale_slider_->Id()) {
            return static_cast<double>(toolbar_scale_slider_->Value());
        }
        return 0.0;
    }

    double ElementRangeMinimum(UiElementId id) const override
    {
        if (id == opacity_slider_->Id()) {
            return static_cast<double>(opacity_slider_->Minimum());
        }
        if (id == toolbar_scale_slider_->Id()) {
            return static_cast<double>(toolbar_scale_slider_->Minimum());
        }
        return 0.0;
    }

    double ElementRangeMaximum(UiElementId id) const override
    {
        if (id == opacity_slider_->Id()) {
            return static_cast<double>(opacity_slider_->Maximum());
        }
        if (id == toolbar_scale_slider_->Id()) {
            return static_cast<double>(toolbar_scale_slider_->Maximum());
        }
        return 0.0;
    }

    double ElementRangeSmallChange(UiElementId id) const override
    {
        if (id == opacity_slider_->Id()) {
            return static_cast<double>(kOpacitySmallStep);
        }
        if (id == toolbar_scale_slider_->Id()) {
            return static_cast<double>(kToolbarScaleSmallStep);
        }
        return 1.0;
    }

    double ElementRangeLargeChange(UiElementId id) const override
    {
        if (id == opacity_slider_->Id()) {
            return static_cast<double>(kOpacityLargeStep);
        }
        if (id == toolbar_scale_slider_->Id()) {
            return static_cast<double>(kToolbarScaleLargeStep);
        }
        return 10.0;
    }

    HRESULT SetElementRangeValue(UiElementId id, double value) override
    {
        if (id == opacity_slider_->Id()) {
            SetOpacityPercent(static_cast<int>(value + 0.5));
            return S_OK;
        }
        if (id == toolbar_scale_slider_->Id()) {
            SetToolbarScalePercent(static_cast<int>(value + 0.5));
            return S_OK;
        }
        return E_NOTIMPL;
    }

    void Draw(const UiDrawContext& context, UiRootState state) override
    {
        const D2D1_SIZE_F size = context.viewport_size;
        Layout(size);
        const UiDraw draw(context);
        draw.Clear(ui_theme::color::kWindowBackground);
        draw.DrawBodyText(L"Settings", 8, layout_.title, ui_theme::color::kBodyText);
        draw.DrawBodyText(L"Window size", 11, layout_.window_size_label, ui_theme::color::kMutedText);
        draw.DrawBodyText(L"Image rendering", 15, layout_.image_rendering_label, ui_theme::color::kMutedText);
        draw.DrawBodyText(L"Window frame", 12, layout_.window_frame_label, ui_theme::color::kMutedText);
        draw.DrawBodyText(L"Opacity", 7, layout_.opacity_label, ui_theme::color::kMutedText);
        draw.DrawBodyText(
            opacity_text_.c_str(),
            static_cast<UINT32>(opacity_text_.size()),
            layout_.opacity_value,
            ui_theme::color::kBodyText,
            D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
        draw.DrawBodyText(L"Toolbar size", 12, layout_.toolbar_scale_label, ui_theme::color::kMutedText);
        draw.DrawBodyText(
            toolbar_scale_text_.c_str(),
            static_cast<UINT32>(toolbar_scale_text_.size()),
            layout_.toolbar_scale_value,
            ui_theme::color::kBodyText,
            D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
        draw.DrawBodyText(L"Shortcut filter", 15, layout_.shortcut_filter_label, ui_theme::color::kMutedText);
        draw.DrawBodyText(L"Action shortcuts", 16, layout_.action_shortcuts_label, ui_theme::color::kMutedText);
        draw.DrawBodyText(
            shortcut_text_.c_str(),
            static_cast<UINT32>(shortcut_text_.size()),
            layout_.shortcut_text,
            ui_theme::color::kBodyText,
            D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);

        for (UiElement* control : controls_) {
            DrawElement(*control, context, state);
        }
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
    template <typename T>
    T* AddControl(std::unique_ptr<T> control)
    {
        T* typed_control = control.get();
        UiElement* element = root_->AddChild(std::move(control));
        controls_.push_back(element);
        return typed_control;
    }

    void Layout(D2D1_SIZE_F size)
    {
        root_->SetRect(D2D1::RectF(0.0f, 0.0f, size.width, size.height));
        layout_ = CalculateSettingsLayout(size);
        remember_checkbox_->SetRect(layout_.remember_checkbox);
        remember_radio_->SetRect(layout_.remember_radio);
        default_radio_->SetRect(layout_.default_radio);
        pixelated_checkbox_->SetRect(layout_.pixelated_checkbox);
        checkerboard_checkbox_->SetRect(layout_.checkerboard_checkbox);
        borderless_checkbox_->SetRect(layout_.borderless_checkbox);
        opacity_slider_->SetRect(layout_.opacity_slider);
        toolbar_scale_slider_->SetRect(layout_.toolbar_scale_slider);
        filter_box_->SetRect(layout_.filter_box);
        action_dropdown_->SetRect(layout_.action_dropdown);
        reset_button_->SetRect(layout_.reset_button);
        save_button_->SetRect(layout_.save_button);
        cancel_button_->SetRect(layout_.cancel_button);
    }

    void ApplyElementEffect(UiElementId id) override
    {
        if (id == remember_checkbox_->Id()) {
            draft_.remember_window_size = !draft_.remember_window_size;
            SyncChoiceControls();
        } else if (id == pixelated_checkbox_->Id()) {
            draft_.pixelated_sampling = !draft_.pixelated_sampling;
            SyncChoiceControls();
        } else if (id == checkerboard_checkbox_->Id()) {
            draft_.checkerboard_background = !draft_.checkerboard_background;
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
        } else if (id == toolbar_scale_slider_->Id()) {
            draft_.toolbar_scale_percent = ClampToolbarScalePercent(toolbar_scale_slider_->Value());
            UpdateToolbarScaleText();
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
        checkerboard_checkbox_->SetChecked(draft_.checkerboard_background);
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

    void UpdateToolbarScaleText()
    {
        wchar_t text[16] = {};
        swprintf_s(text, L"%d%%", draft_.toolbar_scale_percent);
        toolbar_scale_text_ = text;
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
            (id == checkerboard_checkbox_->Id() && checkerboard_checkbox_->IsChecked()) ||
            (id == borderless_checkbox_->Id() && borderless_checkbox_->IsChecked()) ||
            (id == remember_radio_->Id() && remember_radio_->IsSelected()) ||
            (id == default_radio_->Id() && default_radio_->IsSelected());
    }

    ImgViewerConfig draft_;
    std::unique_ptr<UiElement> root_;
    std::vector<UiElement*> controls_;
    Checkbox* remember_checkbox_ = nullptr;
    Checkbox* pixelated_checkbox_ = nullptr;
    Checkbox* checkerboard_checkbox_ = nullptr;
    Checkbox* borderless_checkbox_ = nullptr;
    RadioButton* remember_radio_ = nullptr;
    RadioButton* default_radio_ = nullptr;
    Slider* opacity_slider_ = nullptr;
    Slider* toolbar_scale_slider_ = nullptr;
    Dropdown* action_dropdown_ = nullptr;
    TextBox* filter_box_ = nullptr;
    Button* reset_button_ = nullptr;
    Button* save_button_ = nullptr;
    Button* cancel_button_ = nullptr;
    std::wstring opacity_text_;
    std::wstring toolbar_scale_text_;
    std::wstring shortcut_text_;
    SettingsLayoutRects layout_;
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
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
        if (info != nullptr) {
            RECT min_rect{0, 0, kSettingsMinClientWidth, kSettingsMinClientHeight};
            const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(window_host.Hwnd(), GWL_STYLE));
            const DWORD ex_style = static_cast<DWORD>(GetWindowLongPtrW(window_host.Hwnd(), GWL_EXSTYLE));
            AdjustWindowRectEx(&min_rect, style, FALSE, ex_style);
            info->ptMinTrackSize.x = min_rect.right - min_rect.left;
            info->ptMinTrackSize.y = min_rect.bottom - min_rect.top;
        }
        return win32::WindowMessageResult::Handled();
    }
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
            .body_font_size = 19.0f,
            .icon_font_size = 22.0f,
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
