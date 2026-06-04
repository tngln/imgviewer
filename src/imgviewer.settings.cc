#include "imgviewer.settings.hpp"

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
#include "ui.a11y.hpp"
#include "ui.button.hpp"
#include "ui.draw.hpp"
#include "ui.popup.hpp"
#include "ui.selection.hpp"
#include "ui.textbox.hpp"
#include "ui.theme.hpp"

namespace {

constexpr wchar_t kSettingsClassName[] = L"ImgViewerSettingsWindow";

constexpr std::array<ImgViewerAction, 9> kShownActions{
    ImgViewerAction::OpenImage,
    ImgViewerAction::PreviousImage,
    ImgViewerAction::NextImage,
    ImgViewerAction::ZoomIn,
    ImgViewerAction::ZoomOut,
    ImgViewerAction::RotateClockwise,
    ImgViewerAction::FlipHorizontal,
    ImgViewerAction::FlipVertical,
    ImgViewerAction::ResetView,
};

constexpr wchar_t kSaveIcon[] = L"\xE105";
constexpr wchar_t kCancelIcon[] = L"\xE711";
constexpr wchar_t kResetIcon[] = L"\xE777";
constexpr UINT_PTR kCaretTimerId = 1;

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

class SettingsUi final : public UiAccessibilitySource {
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
        UpdateFilterResults();
        UpdateShortcutText();
    }

    const ImgViewerConfig& Draft() const { return draft_; }

    const wchar_t* AccessibilityRootName() const override { return L"Settings"; }

    size_t ElementCount() const override { return root_->ChildCount(); }

    const UiElementMetadata* ElementMetadataAt(size_t index) const override
    {
        const UiElement* element = root_->ChildAt(index);
        return element != nullptr ? &element->Metadata() : nullptr;
    }

    const UiElementMetadata* ElementMetadata(UiElementId id) const override
    {
        const UiElement* element = root_->FindById(id);
        return element != nullptr && element != root_.get() ? &element->Metadata() : nullptr;
    }

    D2D1_RECT_F ElementRect(UiElementId id) const override
    {
        const UiElement* element = root_->FindById(id);
        return element != nullptr && element != root_.get() ? element->Rect() : D2D1::RectF();
    }

    bool IsElementEnabled(UiElementId id) const override
    {
        const UiElement* element = root_->FindById(id);
        return element != nullptr && element != root_.get() && element->IsEnabled();
    }

    const wchar_t* ElementValue(UiElementId id) const override
    {
        return id == filter_box_->Id() ? filter_box_->Text().c_str() : L"";
    }

    void Draw(const UiDrawContext& context, D2D1_SIZE_F size)
    {
        Layout(size);
        const UiDraw draw(context);
        draw.Clear(ui_theme::color::kWindowBackground);
        draw.DrawBodyText(L"Settings", 8, D2D1::RectF(24.0f, 18.0f, size.width - 24.0f, 46.0f), ui_theme::color::kBodyText);
        draw.DrawBodyText(L"Window size", 11, D2D1::RectF(24.0f, 88.0f, size.width - 24.0f, 112.0f), ui_theme::color::kMutedText);
        draw.DrawBodyText(L"Image rendering", 15, D2D1::RectF(24.0f, 194.0f, size.width - 24.0f, 218.0f), ui_theme::color::kMutedText);
        draw.DrawBodyText(L"Shortcut filter", 15, D2D1::RectF(24.0f, 278.0f, size.width - 24.0f, 302.0f), ui_theme::color::kMutedText);
        draw.DrawBodyText(L"Action shortcuts", 16, D2D1::RectF(24.0f, 348.0f, size.width - 24.0f, 372.0f), ui_theme::color::kMutedText);
        draw.DrawBodyText(
            shortcut_text_.c_str(),
            static_cast<UINT32>(shortcut_text_.size()),
            D2D1::RectF(24.0f, 416.0f, size.width - 24.0f, 442.0f),
            ui_theme::color::kBodyText,
            D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);

        DrawElement(*remember_checkbox_, context);
        DrawElement(*remember_radio_, context);
        DrawElement(*default_radio_, context);
        DrawElement(*pixelated_checkbox_, context);
        DrawElement(*reset_button_, context);
        DrawElement(*save_button_, context);
        DrawElement(*cancel_button_, context);
        DrawElement(*filter_box_, context);
        DrawElement(*action_dropdown_, context);
    }

    UiEventResult OnInputEvent(const UiInputEvent& event)
    {
        switch (event.type) {
        case UiEventType::PointerMove:
        case UiEventType::PointerDown:
        case UiEventType::PointerUp:
        case UiEventType::PointerLeave:
        case UiEventType::PointerWheel:
            return OnPointerEvent(event.pointer);
        case UiEventType::KeyDown:
        case UiEventType::KeyUp:
            return OnKeyEvent(event.key);
        case UiEventType::TextChar:
            return OnTextChar(event.character);
        case UiEventType::ImeStartComposition:
            return IsTextBoxFocused() ? UiEventResult{.handled = true, .wants_ime_position = true} : UiEventResult{};
        case UiEventType::ImeComposition:
            return focused_ == filter_box_->Id() ? filter_box_->OnInputEvent(event) : UiEventResult{};
        case UiEventType::ImeEndComposition:
            return focused_ == filter_box_->Id() ? filter_box_->OnInputEvent(event) : UiEventResult{};
        case UiEventType::ContextMenu:
            return OnContextMenu(event.point, event.hwnd, event.popup_host);
        case UiEventType::Timer:
            if (IsTextBoxFocused()) {
                return filter_box_->OnInputEvent(event);
            }
            return {};
        case UiEventType::Cancel:
            return OnKeyEvent(UiKeyEvent{.type = UiEventType::KeyDown, .virtual_key = VK_ESCAPE, .focused = focused_});
        case UiEventType::OwnerDeactivated:
            action_dropdown_->Collapse();
            return UiEventResult{.handled = true, .needs_render = true};
        default:
            return {};
        }
    }

    UiEventResult OnKeyEvent(const UiKeyEvent& event)
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

        UiElement* focused = focused_ == UiElementId::None ? nullptr : root_->FindById(focused_);
        if (focused == nullptr) {
            return {};
        }
        UiEventResult result = focused->OnInputEvent(UiInputEvent{.type = event.type, .key = event});
        if (result.handled) {
            ApplyElementEffect(focused_);
        }
        return result;
    }

    UiEventResult OnTextChar(wchar_t ch)
    {
        if (focused_ != filter_box_->Id()) {
            return {};
        }
        UiEventResult result = filter_box_->OnInputEvent(UiInputEvent{.type = UiEventType::TextChar, .character = ch});
        if (result.handled) {
            UpdateFilterResults();
            UpdateShortcutText();
        }
        return result;
    }

    UiEventResult OnContextMenu(D2D1_POINT_2F point, HWND hwnd, PopupHost* popup_host)
    {
        if (filter_box_->Contains(point)) {
            focused_ = filter_box_->Id();
            SetFocus(hwnd);
            if (popup_host != nullptr) {
                popup_host->OpenMenu(point, filter_box_->ContextMenuItems());
            }
            return UiEventResult{.handled = true, .needs_render = true, .focus = UiFocusRequest::FocusTarget, .focus_target = filter_box_->Id()};
        }
        return {};
    }

    UiEventResult ExecuteTextAction(ImgViewerAction action, HWND hwnd)
    {
        if (focused_ != filter_box_->Id()) {
            return {};
        }
        UiEventResult result = filter_box_->ExecuteEditAction(action, hwnd);
        if (result.handled) {
            UpdateFilterResults();
            UpdateShortcutText();
        }
        return result;
    }

    void SetCaretVisible(bool visible)
    {
        filter_box_->SetCaretVisible(visible);
    }

    bool IsTextBoxFocused() const
    {
        return focused_ == filter_box_->Id();
    }

    D2D1_POINT_2F TextCaretPoint() const
    {
        return filter_box_->CaretPoint();
    }

private:
    void Layout(D2D1_SIZE_F size)
    {
        root_->SetRect(D2D1::RectF(0.0f, 0.0f, size.width, size.height));
        remember_checkbox_->SetRect(D2D1::RectF(24.0f, 54.0f, size.width - 24.0f, 84.0f));
        remember_radio_->SetRect(D2D1::RectF(44.0f, 116.0f, size.width - 24.0f, 146.0f));
        default_radio_->SetRect(D2D1::RectF(44.0f, 146.0f, size.width - 24.0f, 176.0f));
        pixelated_checkbox_->SetRect(D2D1::RectF(24.0f, 224.0f, size.width - 24.0f, 254.0f));
        filter_box_->SetRect(D2D1::RectF(24.0f, 308.0f, size.width - 24.0f, 342.0f));
        action_dropdown_->SetRect(D2D1::RectF(24.0f, 378.0f, size.width - 24.0f, 412.0f));
        reset_button_->SetRect(D2D1::RectF(24.0f, size.height - 58.0f, 150.0f, size.height - 20.0f));
        cancel_button_->SetRect(D2D1::RectF(size.width - 132.0f, size.height - 58.0f, size.width - 12.0f, size.height - 20.0f));
        save_button_->SetRect(D2D1::RectF(size.width - 254.0f, size.height - 58.0f, size.width - 142.0f, size.height - 20.0f));
    }

    UiEventResult OnPointerEvent(const UiPointerEvent& event)
    {
        if (event.type == UiEventType::PointerLeave) {
            const bool changed = hovered_ != UiElementId::None;
            hovered_ = UiElementId::None;
            return UiEventResult{.handled = changed, .needs_render = changed};
        }

        if (event.type == UiEventType::PointerDown && action_dropdown_->IsExpanded() && !DropdownHitTest(event.point)) {
            action_dropdown_->Collapse();
        }

        const UiElementId hit_id = HitTest(event.point);
        const UiElementId previous_hover = hovered_;
        hovered_ = hit_id;
        const UiElementId target_id = captured_ != UiElementId::None ? captured_ : hit_id;

        UiPointerEvent target_event = event;
        target_event.target = hit_id;
        target_event.captured = captured_;

        UiEventResult result;
        if (UiElement* target = root_->FindById(target_id)) {
            result = target->OnInputEvent(UiInputEvent{.type = target_event.type, .pointer = target_event, .point = target_event.point});
        }

        if (result.capture == UiCaptureRequest::Capture) {
            captured_ = target_id;
            pressed_ = target_id;
            focused_ = target_id;
        } else if (result.capture == UiCaptureRequest::Release) {
            captured_ = UiElementId::None;
            pressed_ = UiElementId::None;
        }

        if (event.type == UiEventType::PointerUp && target_id != UiElementId::None && hit_id == target_id) {
            ApplyElementEffect(target_id);
        }

        result.needs_render = result.needs_render || previous_hover != hovered_;
        return result;
    }

    UiElementId HitTest(D2D1_POINT_2F point) const
    {
        if (DropdownHitTest(point)) {
            return action_dropdown_->Id();
        }
        const UiElement* element = root_->HitTest(point);
        return element != nullptr ? element->Id() : UiElementId::None;
    }

    bool DropdownHitTest(D2D1_POINT_2F point) const
    {
        if (!action_dropdown_->IsExpanded()) {
            return false;
        }
        const D2D1_RECT_F rect = action_dropdown_->Rect();
        return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom + 32.0f * static_cast<float>(kShownActions.size());
    }

    void ApplyElementEffect(UiElementId id)
    {
        if (id == remember_checkbox_->Id()) {
            draft_.remember_window_size = !draft_.remember_window_size;
            SyncChoiceControls();
        } else if (id == pixelated_checkbox_->Id()) {
            draft_.pixelated_sampling = !draft_.pixelated_sampling;
            SyncChoiceControls();
        } else if (id == remember_radio_->Id()) {
            draft_.remember_window_size = true;
            SyncChoiceControls();
        } else if (id == default_radio_->Id()) {
            draft_.remember_window_size = false;
            SyncChoiceControls();
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
    }

    void UpdateShortcutText()
    {
        const ImgViewerAction action = action_dropdown_->SelectedAction();
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

    void DrawElement(UiElement& element, const UiDrawContext& context) const
    {
        element.Draw(
            context,
            UiElementState{
                .hovered = hovered_ == element.Id(),
                .pressed = pressed_ == element.Id(),
                .active = focused_ == element.Id(),
                .enabled = element.IsEnabled(),
                .checked = IsCheckedElement(element.Id()),
                .expanded = element.Id() == action_dropdown_->Id() && action_dropdown_->IsExpanded(),
            });
    }

    bool IsCheckedElement(UiElementId id) const
    {
        return (id == remember_checkbox_->Id() && remember_checkbox_->IsChecked()) ||
            (id == pixelated_checkbox_->Id() && pixelated_checkbox_->IsChecked()) ||
            (id == remember_radio_->Id() && remember_radio_->IsSelected()) ||
            (id == default_radio_->Id() && default_radio_->IsSelected());
    }

    ImgViewerConfig draft_;
    UiElementIdGenerator ids_;
    std::unique_ptr<UiElement> root_;
    Checkbox* remember_checkbox_ = nullptr;
    Checkbox* pixelated_checkbox_ = nullptr;
    RadioButton* remember_radio_ = nullptr;
    RadioButton* default_radio_ = nullptr;
    Dropdown* action_dropdown_ = nullptr;
    TextBox* filter_box_ = nullptr;
    Button* reset_button_ = nullptr;
    Button* save_button_ = nullptr;
    Button* cancel_button_ = nullptr;
    UiElementId hovered_ = UiElementId::None;
    UiElementId pressed_ = UiElementId::None;
    UiElementId captured_ = UiElementId::None;
    UiElementId focused_ = UiElementId::None;
    std::wstring shortcut_text_;
};

struct SettingsWindowContext final {
    HWND owner = nullptr;
    ImgViewerContext* app = nullptr;
    SettingsUi ui;
    wil::com_ptr<ID2D1Factory> d2d_factory;
    wil::com_ptr<ID2D1HwndRenderTarget> render_target;
    wil::com_ptr<IDWriteFactory> dwrite_factory;
    wil::com_ptr<IDWriteTextFormat> body_text_format;
    wil::com_ptr<IDWriteTextFormat> icon_text_format;
    wil::com_ptr<IRawElementProviderSimple> accessibility_provider;
    PopupHost popup_host;
    bool caret_visible = true;

    explicit SettingsWindowContext(ImgViewerConfig config) : ui(std::move(config)) {}
};

SettingsWindowContext* GetSettingsContext(HWND hwnd)
{
    return reinterpret_cast<SettingsWindowContext*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

bool IsKeyDown(int virtual_key)
{
    return (GetKeyState(virtual_key) & 0x8000) != 0;
}

UiModifiers CurrentModifiers()
{
    return UiModifiers{
        .ctrl = IsKeyDown(VK_CONTROL),
        .shift = IsKeyDown(VK_SHIFT),
        .alt = IsKeyDown(VK_MENU),
    };
}

HRESULT EnsureRenderTarget(HWND hwnd, SettingsWindowContext* context)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, context);
    if (context->render_target != nullptr) {
        return S_OK;
    }

    RECT rect = {};
    RETURN_IF_WIN32_BOOL_FALSE(GetClientRect(hwnd, &rect));
    const D2D1_SIZE_U size = D2D1::SizeU(
        static_cast<UINT32>((std::max)(1L, rect.right - rect.left)),
        static_cast<UINT32>((std::max)(1L, rect.bottom - rect.top)));

    RETURN_IF_FAILED(context->d2d_factory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(),
            96.0f,
            96.0f),
        D2D1::HwndRenderTargetProperties(hwnd, size),
        context->render_target.put()));
    return S_OK;
}

HRESULT InitializeRenderer(SettingsWindowContext* context)
{
    RETURN_IF_FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, context->d2d_factory.put()));
    RETURN_IF_FAILED(DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(context->dwrite_factory.put())));
    RETURN_IF_FAILED(context->dwrite_factory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        17.0f,
        L"",
        context->body_text_format.put()));
    RETURN_IF_FAILED(context->dwrite_factory->CreateTextFormat(
        L"Segoe MDL2 Assets",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        20.0f,
        L"",
        context->icon_text_format.put()));
    RETURN_IF_FAILED(context->body_text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
    RETURN_IF_FAILED(context->icon_text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
    context->popup_host.SetTextFormats(context->body_text_format.get(), context->icon_text_format.get());
    return S_OK;
}

void RenderSettings(HWND hwnd, SettingsWindowContext* context)
{
    if (context == nullptr || FAILED(EnsureRenderTarget(hwnd, context))) {
        return;
    }

    RECT rect = {};
    GetClientRect(hwnd, &rect);
    const D2D1_SIZE_F size = D2D1::SizeF(
        static_cast<float>((std::max)(1L, rect.right - rect.left)),
        static_cast<float>((std::max)(1L, rect.bottom - rect.top)));
    const UiDrawContext draw_context{
        .d2d_context = context->render_target.get(),
        .dwrite_factory = context->dwrite_factory.get(),
        .body_text_format = context->body_text_format.get(),
        .icon_text_format = context->icon_text_format.get(),
    };

    context->render_target->BeginDraw();
    context->ui.Draw(draw_context, size);
    context->popup_host.Draw(draw_context);
    const HRESULT hr = context->render_target->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        context->render_target.reset();
    }
}

void InvalidateForResult(HWND hwnd, UiEventResult result)
{
    if (result.needs_render) {
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

void SyncCaretTimer(HWND hwnd, SettingsWindowContext* context)
{
    if (context == nullptr || !context->ui.IsTextBoxFocused()) {
        KillTimer(hwnd, kCaretTimerId);
        return;
    }

    context->caret_visible = true;
    context->ui.SetCaretVisible(true);
    const UINT blink_time = GetCaretBlinkTime();
    SetTimer(hwnd, kCaretTimerId, blink_time == INFINITE ? 530 : blink_time, nullptr);
}

std::wstring ImeCompositionString(HWND hwnd, LPARAM lparam)
{
    if ((lparam & GCS_COMPSTR) == 0) {
        return {};
    }
    HIMC ime = ImmGetContext(hwnd);
    if (ime == nullptr) {
        return {};
    }
    const LONG bytes = ImmGetCompositionStringW(ime, GCS_COMPSTR, nullptr, 0);
    std::wstring text(bytes > 0 ? static_cast<size_t>(bytes) / sizeof(wchar_t) : 0, L'\0');
    if (!text.empty()) {
        ImmGetCompositionStringW(ime, GCS_COMPSTR, text.data(), bytes);
    }
    ImmReleaseContext(hwnd, ime);
    return text;
}

void PositionIme(HWND hwnd, SettingsWindowContext* context)
{
    if (context == nullptr || !context->ui.IsTextBoxFocused()) {
        return;
    }
    HIMC ime = ImmGetContext(hwnd);
    if (ime == nullptr) {
        return;
    }
    const D2D1_POINT_2F point = context->ui.TextCaretPoint();
    COMPOSITIONFORM form = {};
    form.dwStyle = CFS_POINT;
    form.ptCurrentPos = POINT{static_cast<LONG>(point.x), static_cast<LONG>(point.y)};
    ImmSetCompositionWindow(ime, &form);
    ImmReleaseContext(hwnd, ime);
}

void SaveSettings(HWND hwnd, SettingsWindowContext* context)
{
    if (context->app != nullptr) {
        context->app->config = context->ui.Draft();
        context->app->viewer.SetPixelatedSampling(context->app->config.pixelated_sampling);
        SaveImgViewerConfig(context->app->config);
        RenderImgViewer(context->app);
    }
    DestroyWindow(hwnd);
}

void ExecuteSettingsAction(HWND hwnd, SettingsWindowContext* context, ImgViewerAction action)
{
    switch (action) {
    case ImgViewerAction::TextCopy:
    case ImgViewerAction::TextCut:
    case ImgViewerAction::TextPaste:
    case ImgViewerAction::TextSelectAll:
        if (context != nullptr) {
            InvalidateForResult(hwnd, context->ui.ExecuteTextAction(action, hwnd));
        }
        break;
    case ImgViewerAction::SaveSettings:
        SaveSettings(hwnd, context);
        break;
    case ImgViewerAction::CloseSettings:
        DestroyWindow(hwnd);
        break;
    case ImgViewerAction::ResetKeyBindings:
        if (context != nullptr) {
            ImgViewerConfig draft = context->ui.Draft();
            draft.action_bindings = DefaultActionBindings();
            context->ui = SettingsUi(std::move(draft));
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        break;
    default:
        break;
    }
}

bool DispatchPopupPointer(
    HWND hwnd,
    SettingsWindowContext* context,
    UiEventType type,
    D2D1_POINT_2F point,
    UiPointerButton button = UiPointerButton::None)
{
    if (context == nullptr || !context->popup_host.IsOpen()) {
        return false;
    }

    UiPointerEvent pointer{
        .type = type,
        .point = point,
        .button = button,
    };
    const UiEventResult result = context->popup_host.OnInputEvent(UiInputEvent{
        .type = type,
        .pointer = pointer,
        .point = point,
        .hwnd = hwnd,
    });
    InvalidateForResult(hwnd, result);
    ExecuteSettingsAction(hwnd, context, result.action);
    return result.handled;
}

LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_NCCREATE: {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return TRUE;
    }
    case WM_CREATE: {
        auto* context = GetSettingsContext(hwnd);
        return context != nullptr &&
                SUCCEEDED(InitializeRenderer(context)) &&
                SUCCEEDED(context->popup_host.Initialize(hwnd, context->d2d_factory.get(), context->dwrite_factory.get())) &&
                SUCCEEDED(CreateUiAccessibilityProvider(hwnd, &context->ui, context->accessibility_provider.put()))
            ? 0
            : -1;
    }
    case kImgViewerUiActionMessage:
        ExecuteSettingsAction(hwnd, GetSettingsContext(hwnd), static_cast<ImgViewerAction>(wparam));
        return 0;
    case WM_SIZE: {
        auto* context = GetSettingsContext(hwnd);
        if (context != nullptr && context->render_target != nullptr) {
            context->render_target->Resize(D2D1::SizeU(LOWORD(lparam), HIWORD(lparam)));
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT paint = {};
        BeginPaint(hwnd, &paint);
        RenderSettings(hwnd, GetSettingsContext(hwnd));
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE: {
        auto* context = GetSettingsContext(hwnd);
        TRACKMOUSEEVENT track = {.cbSize = sizeof(track), .dwFlags = TME_LEAVE, .hwndTrack = hwnd};
        TrackMouseEvent(&track);
        if (context != nullptr) {
            const D2D1_POINT_2F point = D2D1::Point2F(static_cast<float>(GET_X_LPARAM(lparam)), static_cast<float>(GET_Y_LPARAM(lparam)));
            if (DispatchPopupPointer(hwnd, context, UiEventType::PointerMove, point)) {
                return 0;
            }
            UiPointerEvent pointer{.type = UiEventType::PointerMove, .point = point, .modifiers = CurrentModifiers()};
            InvalidateForResult(hwnd, context->ui.OnInputEvent(UiInputEvent{.type = pointer.type, .pointer = pointer, .point = point, .hwnd = hwnd}));
            PositionIme(hwnd, context);
        }
        return 0;
    }
    case WM_MOUSELEAVE: {
        auto* context = GetSettingsContext(hwnd);
        if (context != nullptr) {
            UiPointerEvent pointer{.type = UiEventType::PointerLeave, .modifiers = CurrentModifiers()};
            InvalidateForResult(hwnd, context->ui.OnInputEvent(UiInputEvent{.type = pointer.type, .pointer = pointer, .hwnd = hwnd}));
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        auto* context = GetSettingsContext(hwnd);
        if (context != nullptr) {
            SetFocus(hwnd);
            const D2D1_POINT_2F point = D2D1::Point2F(static_cast<float>(GET_X_LPARAM(lparam)), static_cast<float>(GET_Y_LPARAM(lparam)));
            if (DispatchPopupPointer(hwnd, context, UiEventType::PointerDown, point, UiPointerButton::Left)) {
                return 0;
            }
            UiPointerEvent pointer{.type = UiEventType::PointerDown, .point = point, .button = UiPointerButton::Left, .modifiers = CurrentModifiers()};
            const UiEventResult result = context->ui.OnInputEvent(UiInputEvent{.type = pointer.type, .pointer = pointer, .point = point, .hwnd = hwnd});
            if (result.capture == UiCaptureRequest::Capture) {
                SetCapture(hwnd);
            }
            InvalidateForResult(hwnd, result);
            SyncCaretTimer(hwnd, context);
            PositionIme(hwnd, context);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        auto* context = GetSettingsContext(hwnd);
        if (context != nullptr) {
            const D2D1_POINT_2F point = D2D1::Point2F(static_cast<float>(GET_X_LPARAM(lparam)), static_cast<float>(GET_Y_LPARAM(lparam)));
            if (DispatchPopupPointer(hwnd, context, UiEventType::PointerUp, point, UiPointerButton::Left)) {
                return 0;
            }
            UiPointerEvent pointer{.type = UiEventType::PointerUp, .point = point, .button = UiPointerButton::Left, .modifiers = CurrentModifiers()};
            const UiEventResult result = context->ui.OnInputEvent(UiInputEvent{.type = pointer.type, .pointer = pointer, .point = point, .hwnd = hwnd});
            if (result.capture == UiCaptureRequest::Release) {
                ReleaseCapture();
            }
            InvalidateForResult(hwnd, result);
            ExecuteSettingsAction(hwnd, context, result.action);
            SyncCaretTimer(hwnd, context);
            PositionIme(hwnd, context);
        }
        return 0;
    }
    case WM_KEYDOWN: {
        auto* context = GetSettingsContext(hwnd);
        if (context != nullptr) {
            if (context->popup_host.IsOpen()) {
                UiKeyEvent key{
                    .type = UiEventType::KeyDown,
                    .virtual_key = static_cast<UINT>(wparam),
                    .modifiers = CurrentModifiers(),
                };
                const UiEventResult popup_result = context->popup_host.OnInputEvent(UiInputEvent{.type = key.type, .key = key, .hwnd = hwnd});
                InvalidateForResult(hwnd, popup_result);
                ExecuteSettingsAction(hwnd, context, popup_result.action);
                if (popup_result.handled) {
                    return 0;
                }
            }
            UiKeyEvent key{
                .type = UiEventType::KeyDown,
                .virtual_key = static_cast<UINT>(wparam),
                .modifiers = CurrentModifiers(),
            };
            const UiEventResult result = context->ui.OnInputEvent(UiInputEvent{.type = key.type, .key = key, .hwnd = hwnd});
            InvalidateForResult(hwnd, result);
            ExecuteSettingsAction(hwnd, context, result.action);
            SyncCaretTimer(hwnd, context);
            PositionIme(hwnd, context);
            if (result.handled) {
                return 0;
            }
        }
        break;
    }
    case WM_CHAR: {
        auto* context = GetSettingsContext(hwnd);
        if (context != nullptr) {
            const UiEventResult result = context->ui.OnInputEvent(UiInputEvent{
                .type = UiEventType::TextChar,
                .character = static_cast<wchar_t>(wparam),
                .hwnd = hwnd,
            });
            InvalidateForResult(hwnd, result);
            PositionIme(hwnd, context);
            if (result.handled) {
                return 0;
            }
        }
        break;
    }
    case WM_IME_STARTCOMPOSITION: {
        auto* context = GetSettingsContext(hwnd);
        if (context != nullptr) {
            InvalidateForResult(hwnd, context->ui.OnInputEvent(UiInputEvent{.type = UiEventType::ImeStartComposition, .hwnd = hwnd}));
            PositionIme(hwnd, context);
        }
        return 0;
    }
    case WM_IME_COMPOSITION: {
        auto* context = GetSettingsContext(hwnd);
        if (context != nullptr) {
            InvalidateForResult(hwnd, context->ui.OnInputEvent(UiInputEvent{
                .type = UiEventType::ImeComposition,
                .text = ImeCompositionString(hwnd, lparam),
                .hwnd = hwnd,
            }));
            PositionIme(hwnd, context);
        }
        break;
    }
    case WM_IME_ENDCOMPOSITION: {
        auto* context = GetSettingsContext(hwnd);
        if (context != nullptr) {
            InvalidateForResult(hwnd, context->ui.OnInputEvent(UiInputEvent{.type = UiEventType::ImeEndComposition, .hwnd = hwnd}));
        }
        return 0;
    }
    case WM_CONTEXTMENU: {
        auto* context = GetSettingsContext(hwnd);
        if (context != nullptr) {
            POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            if (point.x == -1 && point.y == -1) {
                point = POINT{32, 224};
            } else {
                ScreenToClient(hwnd, &point);
            }
            const D2D1_POINT_2F client_point = D2D1::Point2F(static_cast<float>(point.x), static_cast<float>(point.y));
            const UiEventResult result = context->ui.OnInputEvent(UiInputEvent{
                .type = UiEventType::ContextMenu,
                .point = client_point,
                .screen_point = POINT{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)},
                .hwnd = hwnd,
                .popup_host = &context->popup_host,
            });
            InvalidateForResult(hwnd, result);
            SyncCaretTimer(hwnd, context);
            if (result.handled) {
                return 0;
            }
        }
        break;
    }
    case WM_TIMER: {
        if (wparam == kCaretTimerId) {
            auto* context = GetSettingsContext(hwnd);
            if (context != nullptr && context->ui.IsTextBoxFocused()) {
                const UiEventResult result = context->ui.OnInputEvent(UiInputEvent{
                    .type = UiEventType::Timer,
                    .timer_id = static_cast<UINT_PTR>(wparam),
                    .hwnd = hwnd,
                });
                InvalidateForResult(hwnd, result);
            }
            return 0;
        }
        break;
    }
    case WM_ACTIVATE: {
        if (LOWORD(wparam) == WA_INACTIVE) {
            if (auto* context = GetSettingsContext(hwnd)) {
                InvalidateForResult(hwnd, context->popup_host.OnInputEvent(UiInputEvent{.type = UiEventType::OwnerDeactivated, .hwnd = hwnd}));
                InvalidateForResult(hwnd, context->ui.OnInputEvent(UiInputEvent{.type = UiEventType::OwnerDeactivated, .hwnd = hwnd}));
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        break;
    }
    case WM_GETOBJECT: {
        if (lparam == UiaRootObjectId) {
            auto* context = GetSettingsContext(hwnd);
            if (context != nullptr) {
                return UiaReturnRawElementProvider(
                    hwnd,
                    wparam,
                    lparam,
                    context->accessibility_provider.get());
            }
        }
        break;
    }
    case WM_CLOSE:
        if (auto* context = GetSettingsContext(hwnd)) {
            context->popup_host.Close();
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY: {
        auto* context = GetSettingsContext(hwnd);
        if (context != nullptr) {
            if (context->app != nullptr) {
                context->app->settings_window = nullptr;
            }
            context->popup_host.Close();
            delete context;
        }
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return 0;
    }
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

HRESULT RegisterSettingsWindowClass(HINSTANCE instance)
{
    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = SettingsWindowProc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = kSettingsClassName;
    const ATOM atom = RegisterClassExW(&window_class);
    if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        RETURN_LAST_ERROR();
    }
    return S_OK;
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
    RETURN_IF_FAILED(RegisterSettingsWindowClass(instance));

    auto* settings_context = new (std::nothrow) SettingsWindowContext(context->config);
    RETURN_IF_NULL_ALLOC(settings_context);
    settings_context->owner = owner;
    settings_context->app = context;

    HWND window = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        kSettingsClassName,
        L"Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        720,
        560,
        owner,
        nullptr,
        instance,
        settings_context);
    if (window == nullptr) {
        delete settings_context;
        RETURN_LAST_ERROR();
    }

    context->settings_window = window;
    ShowWindow(window, SW_SHOWNORMAL);
    UpdateWindow(window);
    return S_OK;
}
