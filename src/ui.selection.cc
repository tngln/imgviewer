#include "ui.selection.hpp"

#include <algorithm>
#include <cwchar>
#include <utility>

#include <d2d1helper.h>

#include "math.hpp"
#include "ui.theme.hpp"

namespace {

constexpr float kChoiceMarkSize = 18.0f;
constexpr float kChoiceGap = 10.0f;
constexpr float kChoiceTextTop = 4.0f;
constexpr float kDropdownItemHeight = 32.0f;

D2D1_COLOR_F ControlFill(UiElementState state)
{
    if (!state.enabled) {
        return ui_theme::color::kButtonDisabled;
    }
    if (state.pressed) {
        return ui_theme::color::kButtonPressed;
    }
    if (state.hovered || state.expanded) {
        return ui_theme::color::kButtonHovered;
    }
    return ui_theme::color::kButtonDefault;
}

UiEventResult ChoicePointerEvent(UiElement& element, const UiPointerEvent& event)
{
    if (event.button != UiPointerButton::Left &&
        (event.type == UiEventType::PointerDown || event.type == UiEventType::PointerUp)) {
        return {};
    }

    if (event.type == UiEventType::PointerDown) {
        const bool enabled = element.IsEnabled();
        return UiEventResult{
            .handled = true,
            .needs_render = enabled,
            .capture = enabled ? UiCaptureRequest::Capture : UiCaptureRequest::None,
            .focus = enabled ? UiFocusRequest::FocusTarget : UiFocusRequest::None,
            .focus_target = enabled ? element.Id() : UiElementId::None,
        };
    }

    if (event.type == UiEventType::PointerUp && event.captured == element.Id()) {
        return UiEventResult{
            .handled = true,
            .needs_render = element.IsEnabled(),
            .capture = UiCaptureRequest::Release,
            .action = element.IsEnabled() && event.target == element.Id() ? element.Action() : kUiActionNone,
        };
    }

    return {};
}

UiEventResult ChoiceKeyEvent(UiElement& element, const UiKeyEvent& event)
{
    if (event.type != UiEventType::KeyDown || !element.IsEnabled()) {
        return {};
    }
    if (event.virtual_key != VK_RETURN && event.virtual_key != VK_SPACE) {
        return {};
    }
    return UiEventResult{
        .handled = true,
        .needs_render = true,
        .action = element.Action(),
    };
}

} // namespace

Checkbox::Checkbox(UiElementMetadata metadata, const wchar_t* text, bool checked) :
    UiElement(metadata),
    text_(text),
    checked_(checked)
{
    SetFocusable(true);
}

bool Checkbox::IsChecked() const
{
    return checked_;
}

void Checkbox::SetChecked(bool checked)
{
    checked_ = checked;
}

void Checkbox::Draw(const UiDrawContext& context, UiElementState state) const
{
    const UiDraw draw(context);
    const D2D1_RECT_F rect = Rect();
    const D2D1_RECT_F box = D2D1::RectF(rect.left, rect.top + 5.0f, rect.left + kChoiceMarkSize, rect.top + 5.0f + kChoiceMarkSize);
    draw.FillRoundedRect(D2D1::RoundedRect(box, 3.0f, 3.0f), checked_ ? ui_theme::color::kAccent : ControlFill(state));
    draw.DrawRoundedRect(D2D1::RoundedRect(box, 3.0f, 3.0f), ui_theme::color::kBorder);
    if (checked_) {
        draw.DrawBodyText(L"\x2713", 1, D2D1::RectF(box.left + 2.0f, box.top - 4.0f, box.right, box.bottom), ui_theme::color::kButtonDefault);
    }
    draw.DrawBodyText(
        text_,
        static_cast<UINT32>(wcslen(text_)),
        D2D1::RectF(box.right + kChoiceGap, rect.top + kChoiceTextTop, rect.right, rect.bottom),
        state.enabled ? ui_theme::color::kBodyText : ui_theme::color::kButtonDisabledContent,
        D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
}

UiEventResult Checkbox::OnPointerEvent(const UiPointerEvent& event)
{
    return ChoicePointerEvent(*this, event);
}

UiEventResult Checkbox::OnKeyEvent(const UiKeyEvent& event)
{
    return ChoiceKeyEvent(*this, event);
}

RadioButton::RadioButton(UiElementMetadata metadata, const wchar_t* text, bool selected) :
    UiElement(metadata),
    text_(text),
    selected_(selected)
{
    SetFocusable(true);
}

bool RadioButton::IsSelected() const
{
    return selected_;
}

void RadioButton::SetSelected(bool selected)
{
    selected_ = selected;
}

void RadioButton::Draw(const UiDrawContext& context, UiElementState state) const
{
    const UiDraw draw(context);
    const D2D1_RECT_F rect = Rect();
    const D2D1_RECT_F outer = D2D1::RectF(rect.left, rect.top + 5.0f, rect.left + kChoiceMarkSize, rect.top + 5.0f + kChoiceMarkSize);
    draw.FillRoundedRect(D2D1::RoundedRect(outer, 9.0f, 9.0f), ControlFill(state));
    draw.DrawRoundedRect(D2D1::RoundedRect(outer, 9.0f, 9.0f), selected_ ? ui_theme::color::kAccent : ui_theme::color::kBorder, 1.5f);
    if (selected_) {
        const D2D1_RECT_F inner = D2D1::RectF(outer.left + 5.0f, outer.top + 5.0f, outer.right - 5.0f, outer.bottom - 5.0f);
        draw.FillRoundedRect(D2D1::RoundedRect(inner, 4.0f, 4.0f), ui_theme::color::kAccent);
    }
    draw.DrawBodyText(
        text_,
        static_cast<UINT32>(wcslen(text_)),
        D2D1::RectF(outer.right + kChoiceGap, rect.top + kChoiceTextTop, rect.right, rect.bottom),
        state.enabled ? ui_theme::color::kBodyText : ui_theme::color::kButtonDisabledContent,
        D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
}

UiEventResult RadioButton::OnPointerEvent(const UiPointerEvent& event)
{
    return ChoicePointerEvent(*this, event);
}

UiEventResult RadioButton::OnKeyEvent(const UiKeyEvent& event)
{
    return ChoiceKeyEvent(*this, event);
}

Dropdown::Dropdown(UiElementMetadata metadata, std::vector<DropdownOption> options) :
    UiElement(metadata),
    options_(std::move(options))
{
    SetFocusable(true);
}

size_t Dropdown::SelectedIndex() const
{
    return selected_index_;
}

UiAction Dropdown::SelectedAction() const
{
    return selected_index_ < options_.size() ? options_[selected_index_].action : kUiActionNone;
}

void Dropdown::SetSelectedIndex(size_t index)
{
    if (!options_.empty()) {
        selected_index_ = (std::min)(index, options_.size() - 1);
    }
}

void Dropdown::SetOptions(std::vector<DropdownOption> options)
{
    options_ = std::move(options);
    selected_index_ = options_.empty() ? 0 : (std::min)(selected_index_, options_.size() - 1);
    hovered_index_ = options_.size();
    expanded_ = false;
}

bool Dropdown::IsExpanded() const
{
    return expanded_;
}

void Dropdown::Collapse()
{
    expanded_ = false;
    hovered_index_ = options_.size();
}

void Dropdown::Draw(const UiDrawContext& context, UiElementState state) const
{
    const UiDraw draw(context);
    const D2D1_RECT_F rect = Rect();
    draw.FillRoundedRect(D2D1::RoundedRect(rect, ui_theme::metrics::kButtonCornerRadius, ui_theme::metrics::kButtonCornerRadius), ControlFill(state));
    draw.DrawRoundedRect(D2D1::RoundedRect(rect, ui_theme::metrics::kButtonCornerRadius, ui_theme::metrics::kButtonCornerRadius), ui_theme::color::kBorder);
    const wchar_t* text = options_.empty() ? L"" : options_[selected_index_].text;
    draw.DrawBodyText(text, static_cast<UINT32>(wcslen(text)), D2D1::RectF(rect.left + 12.0f, rect.top + 5.0f, rect.right - 34.0f, rect.bottom), ui_theme::color::kBodyText, D2D1_DRAW_TEXT_OPTIONS_CLIP);
    draw.DrawBodyText(expanded_ ? L"\x2303" : L"\x2304", 1, D2D1::RectF(rect.right - 26.0f, rect.top + 1.0f, rect.right, rect.bottom), ui_theme::color::kMutedText);
    if (!expanded_) {
        return;
    }

    for (size_t index = 0; index < options_.size(); ++index) {
        const D2D1_RECT_F option_rect = OptionRect(index);
        draw.FillRect(
            option_rect,
            index == hovered_index_ || index == selected_index_ ? ui_theme::color::kButtonHovered : ui_theme::color::kButtonDefault);
        draw.DrawBodyText(options_[index].text, static_cast<UINT32>(wcslen(options_[index].text)), D2D1::RectF(option_rect.left + 12.0f, option_rect.top + 3.0f, option_rect.right - 8.0f, option_rect.bottom), ui_theme::color::kBodyText, D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
    const D2D1_RECT_F last = OptionRect(options_.empty() ? 0 : options_.size() - 1);
    draw.DrawRect(D2D1::RectF(rect.left, rect.bottom, rect.right, last.bottom), ui_theme::color::kBorder);
}

UiEventResult Dropdown::OnPointerEvent(const UiPointerEvent& event)
{
    if (!IsEnabled()) {
        return {};
    }

    if (event.type == UiEventType::PointerMove && expanded_ && event.target == Id()) {
        const size_t previous_hovered = hovered_index_;
        hovered_index_ = OptionAt(event.point);
        return UiEventResult{
            .handled = true,
            .needs_render = previous_hovered != hovered_index_,
        };
    }

    if (event.button != UiPointerButton::Left) {
        return {};
    }
    if (event.type == UiEventType::PointerDown) {
        return UiEventResult{
            .handled = true,
            .needs_render = true,
            .capture = UiCaptureRequest::Capture,
            .focus = UiFocusRequest::FocusTarget,
            .focus_target = Id(),
        };
    }
    if (event.type == UiEventType::PointerUp && event.captured == Id()) {
        UiAction action = kUiActionNone;
        if (expanded_ && event.target == Id()) {
            const size_t option = OptionAt(event.point);
            if (option < options_.size()) {
                selected_index_ = option;
                hovered_index_ = option;
                action = options_[option].action;
            }
        }
        expanded_ = !expanded_ && event.target == Id();
        if (expanded_) {
            hovered_index_ = selected_index_;
        } else {
            hovered_index_ = options_.size();
        }
        return UiEventResult{
            .handled = true,
            .needs_render = true,
            .capture = UiCaptureRequest::Release,
            .action = action,
        };
    }
    return {};
}

UiEventResult Dropdown::OnKeyEvent(const UiKeyEvent& event)
{
    if (event.type != UiEventType::KeyDown || !IsEnabled()) {
        return {};
    }
    if (event.virtual_key == VK_ESCAPE) {
        Collapse();
        return UiEventResult{.handled = true, .needs_render = true};
    }
    if (event.virtual_key == VK_RETURN || event.virtual_key == VK_SPACE) {
        expanded_ = !expanded_;
        hovered_index_ = expanded_ ? selected_index_ : options_.size();
        return UiEventResult{.handled = true, .needs_render = true};
    }
    if ((event.virtual_key == VK_DOWN || event.virtual_key == VK_UP) && !options_.empty()) {
        if (event.virtual_key == VK_DOWN) {
            selected_index_ = (std::min)(selected_index_ + 1, options_.size() - 1);
        } else {
            selected_index_ = selected_index_ == 0 ? 0 : selected_index_ - 1;
        }
        hovered_index_ = selected_index_;
        return UiEventResult{.handled = true, .needs_render = true, .action = options_[selected_index_].action};
    }
    return {};
}

size_t Dropdown::OptionAt(D2D1_POINT_2F point) const
{
    for (size_t index = 0; index < options_.size(); ++index) {
        if (math::Contains(OptionRect(index), point)) {
            return index;
        }
    }
    return options_.size();
}

D2D1_RECT_F Dropdown::OptionRect(size_t index) const
{
    const D2D1_RECT_F rect = Rect();
    const float top = rect.bottom + kDropdownItemHeight * static_cast<float>(index);
    return D2D1::RectF(rect.left, top, rect.right, top + kDropdownItemHeight);
}
