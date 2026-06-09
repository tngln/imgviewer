#include "ui.selection.hpp"

#include <algorithm>
#include <cwchar>
#include <utility>

#include <d2d1helper.h>

#include "math.hpp"
#include "ui.popup.hpp"
#include "ui.theme.hpp"

namespace {

constexpr float kChoiceMarkSize = 11.0f;
constexpr float kDropdownItemHeight = 20.0f;
constexpr float kCheckboxCornerRadius = 1.5f;
constexpr float kCheckmarkOffset = 1.5f;
constexpr float kRadioOuterCornerRadius = 5.5f;
constexpr float kRadioBorderWidth = 0.75f;
constexpr float kRadioInnerDotCornerRadius = 2.5f;
constexpr float kRadioDotInset = 3.0f;
constexpr float kDropdownTextLeft = 7.0f;
constexpr float kDropdownTextTop = 4.0f;
constexpr float kDropdownTextRight = 19.0f;
constexpr float kDropdownChevronRight = 15.0f;
constexpr float kDropdownOptionTextLeft = 7.0f;
constexpr float kDropdownOptionTextTop = 3.5f;

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

class DropdownPopupContent final : public UiPopupContent {
public:
    explicit DropdownPopupContent(Dropdown* dropdown) : dropdown_(dropdown) {}

    D2D1_SIZE_F Measure(const UiDrawContext&, D2D1_SIZE_F) const override
    {
        return PopupSize();
    }

    D2D1_SIZE_F PopupSize() const
    {
        if (dropdown_ == nullptr) {
            return D2D1::SizeF(1.0f, 1.0f);
        }
        const D2D1_RECT_F rect = dropdown_->Rect();
        const float width = (std::max)(1.0f, rect.right - rect.left);
        const float height = (std::max)(1.0f, kDropdownItemHeight * static_cast<float>(dropdown_->options_.size()));
        return D2D1::SizeF(width, height);
    }

    void Render(const UiDrawContext& context) const override
    {
        if (dropdown_ == nullptr) {
            return;
        }

        const UiDraw draw(context);
        const D2D1_SIZE_F size = PopupSize();
        const D2D1_RECT_F bounds = D2D1::RectF(0.0f, 0.0f, size.width, size.height);
        draw.FillRect(bounds, ui_theme::color::kButtonDefault);

        for (size_t index = 0; index < dropdown_->options_.size(); ++index) {
            const D2D1_RECT_F option_rect = OptionRect(index);
            draw.FillRect(
                option_rect,
                index == dropdown_->hovered_index_ || index == dropdown_->selected_index_
                    ? ui_theme::color::kButtonHovered
                    : ui_theme::color::kButtonDefault);
            draw.DrawBodyText(
                dropdown_->options_[index].text,
                static_cast<UINT32>(wcslen(dropdown_->options_[index].text)),
                D2D1::RectF(option_rect.left + kDropdownOptionTextLeft, option_rect.top + kDropdownOptionTextTop, option_rect.right - ui_theme::metrics::kSmallGap, option_rect.bottom),
                ui_theme::color::kBodyText,
                D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
        draw.DrawRect(bounds, ui_theme::color::kBorder);
    }

    UiEventResult OnInputEvent(const UiInputEvent& event) override
    {
        if (dropdown_ == nullptr) {
            return {};
        }
        switch (event.type) {
        case UiEventType::PointerMove:
            return OnPointerMove(event.point);
        case UiEventType::PointerDown:
            return SelectOptionAt(event.point);
        case UiEventType::PointerUp:
            return SelectOptionAt(event.point);
        case UiEventType::KeyDown:
            return OnKeyDown(event.key.virtual_key);
        default:
            return {};
        }
    }

    void OnClosed() override
    {
        if (dropdown_ != nullptr) {
            dropdown_->Collapse();
        }
    }

private:
    UiEventResult OnPointerMove(D2D1_POINT_2F point)
    {
        const size_t previous_hovered = dropdown_->hovered_index_;
        dropdown_->hovered_index_ = OptionAt(point);
        return UiEventResult{
            .handled = true,
            .needs_render = previous_hovered != dropdown_->hovered_index_,
        };
    }

    UiEventResult SelectOptionAt(D2D1_POINT_2F point)
    {
        const size_t option = OptionAt(point);
        if (option >= dropdown_->options_.size()) {
            return UiEventResult{.handled = true};
        }

        dropdown_->selected_index_ = option;
        dropdown_->hovered_index_ = option;
        return UiEventResult{
            .handled = true,
            .needs_render = true,
            .action = dropdown_->options_[option].action,
            .close_popup = true,
            .effect_target = dropdown_->Id(),
        };
    }

    UiEventResult OnKeyDown(UINT virtual_key)
    {
        if (virtual_key != VK_DOWN && virtual_key != VK_UP && virtual_key != VK_RETURN && virtual_key != VK_SPACE) {
            return UiEventResult{.handled = true};
        }
        if (dropdown_->options_.empty()) {
            return UiEventResult{.handled = true};
        }
        if (virtual_key == VK_DOWN || virtual_key == VK_UP) {
            if (virtual_key == VK_DOWN) {
                dropdown_->selected_index_ = (std::min)(dropdown_->selected_index_ + 1, dropdown_->options_.size() - 1);
            } else {
                dropdown_->selected_index_ = dropdown_->selected_index_ == 0 ? 0 : dropdown_->selected_index_ - 1;
            }
            dropdown_->hovered_index_ = dropdown_->selected_index_;
            return UiEventResult{
                .handled = true,
                .needs_render = true,
                .action = dropdown_->options_[dropdown_->selected_index_].action,
                .effect_target = dropdown_->Id(),
            };
        }

        return UiEventResult{
            .handled = true,
            .needs_render = true,
            .action = dropdown_->options_[dropdown_->selected_index_].action,
            .close_popup = true,
            .effect_target = dropdown_->Id(),
        };
    }

    size_t OptionAt(D2D1_POINT_2F point) const
    {
        for (size_t index = 0; index < dropdown_->options_.size(); ++index) {
            if (math::Contains(OptionRect(index), point)) {
                return index;
            }
        }
        return dropdown_->options_.size();
    }

    D2D1_RECT_F OptionRect(size_t index) const
    {
        const D2D1_SIZE_F size = PopupSize();
        const float top = kDropdownItemHeight * static_cast<float>(index);
        return D2D1::RectF(0.0f, top, size.width, top + kDropdownItemHeight);
    }

    Dropdown* dropdown_ = nullptr;
};

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

D2D1_SIZE_F Checkbox::Measure(const UiDrawContext&, D2D1_SIZE_F available_size) const
{
    return D2D1::SizeF((std::max)(1.0f, available_size.width), ui_theme::metrics::kWidgetRowHeight);
}

void Checkbox::Render(const UiDrawContext& context, UiRootState root_state) const
{
    const UiElementState state = VisualState(root_state);
    const UiDraw draw(context);
    const D2D1_RECT_F rect = Rect();
    const D2D1_RECT_F box = D2D1::RectF(rect.left, rect.top + ui_theme::metrics::kTextRowTopOffset, rect.left + kChoiceMarkSize, rect.top + ui_theme::metrics::kTextRowTopOffset + kChoiceMarkSize);
    draw.FillRoundedRect(D2D1::RoundedRect(box, kCheckboxCornerRadius, kCheckboxCornerRadius), checked_ ? ui_theme::color::kAccent : ui_theme::WidgetFillColor(state));
    draw.DrawRoundedRect(D2D1::RoundedRect(box, kCheckboxCornerRadius, kCheckboxCornerRadius), ui_theme::color::kBorder, ui_theme::metrics::kStrokeWidth);
    if (checked_) {
        draw.DrawBodyText(L"\x2713", 1, D2D1::RectF(box.left + kCheckmarkOffset, box.top - kCheckmarkOffset, box.right, box.bottom), ui_theme::color::kButtonDefault);
    }
    draw.DrawBodyText(
        text_,
        static_cast<UINT32>(wcslen(text_)),
        D2D1::RectF(box.right + ui_theme::metrics::kStandardGap, rect.top + ui_theme::metrics::kTextTopOffset, rect.right, rect.bottom),
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

D2D1_SIZE_F RadioButton::Measure(const UiDrawContext&, D2D1_SIZE_F available_size) const
{
    return D2D1::SizeF((std::max)(1.0f, available_size.width), ui_theme::metrics::kWidgetRowHeight);
}

void RadioButton::Render(const UiDrawContext& context, UiRootState root_state) const
{
    const UiElementState state = VisualState(root_state);
    const UiDraw draw(context);
    const D2D1_RECT_F rect = Rect();
    const D2D1_RECT_F outer = D2D1::RectF(rect.left, rect.top + ui_theme::metrics::kTextRowTopOffset, rect.left + kChoiceMarkSize, rect.top + ui_theme::metrics::kTextRowTopOffset + kChoiceMarkSize);
    draw.FillRoundedRect(D2D1::RoundedRect(outer, kRadioOuterCornerRadius, kRadioOuterCornerRadius), ui_theme::WidgetFillColor(state));
    draw.DrawRoundedRect(D2D1::RoundedRect(outer, kRadioOuterCornerRadius, kRadioOuterCornerRadius), selected_ ? ui_theme::color::kAccent : ui_theme::color::kBorder, kRadioBorderWidth);
    if (selected_) {
        const D2D1_RECT_F inner = D2D1::RectF(outer.left + kRadioDotInset, outer.top + kRadioDotInset, outer.right - kRadioDotInset, outer.bottom - kRadioDotInset);
        draw.FillRoundedRect(D2D1::RoundedRect(inner, kRadioInnerDotCornerRadius, kRadioInnerDotCornerRadius), ui_theme::color::kAccent);
    }
    draw.DrawBodyText(
        text_,
        static_cast<UINT32>(wcslen(text_)),
        D2D1::RectF(outer.right + ui_theme::metrics::kStandardGap, rect.top + ui_theme::metrics::kTextTopOffset, rect.right, rect.bottom),
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

D2D1_SIZE_F Dropdown::Measure(const UiDrawContext&, D2D1_SIZE_F available_size) const
{
    return D2D1::SizeF((std::max)(1.0f, available_size.width), ui_theme::metrics::kInputHeight);
}

void Dropdown::Render(const UiDrawContext& context, UiRootState root_state) const
{
    UiElementState state = VisualState(root_state);
    state.expanded = expanded_;
    const UiDraw draw(context);
    const D2D1_RECT_F rect = Rect();
    draw.FillRoundedRect(D2D1::RoundedRect(rect, ui_theme::metrics::kButtonCornerRadius, ui_theme::metrics::kButtonCornerRadius), ui_theme::WidgetFillColor(state));
    draw.DrawRoundedRect(D2D1::RoundedRect(rect, ui_theme::metrics::kButtonCornerRadius, ui_theme::metrics::kButtonCornerRadius), ui_theme::color::kBorder);
    const wchar_t* text = options_.empty() ? L"" : options_[selected_index_].text;
    draw.DrawBodyText(text, static_cast<UINT32>(wcslen(text)), D2D1::RectF(rect.left + kDropdownTextLeft, rect.top + kDropdownTextTop, rect.right - kDropdownTextRight, rect.bottom), ui_theme::color::kBodyText, D2D1_DRAW_TEXT_OPTIONS_CLIP);
    draw.DrawBodyText(expanded_ ? L"\x2303" : L"\x2304", 1, D2D1::RectF(rect.right - kDropdownChevronRight, rect.top + ui_theme::metrics::kTextTopOffset, rect.right, rect.bottom), ui_theme::color::kMutedText);
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
        if (!expanded_ && event.target == Id()) {
            if (FAILED(OpenPopup(event.popup_host))) {
                Collapse();
            }
        }
        return UiEventResult{
            .handled = true,
            .needs_render = true,
            .capture = UiCaptureRequest::Release,
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
        if (expanded_) {
            if (event.popup_host != nullptr) {
                event.popup_host->Close();
            }
        } else if (FAILED(OpenPopup(event.popup_host))) {
            Collapse();
        }
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

HRESULT Dropdown::OpenPopup(PopupHost* popup_host)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, popup_host);
    expanded_ = true;
    hovered_index_ = selected_index_;
    return popup_host->Open(
        D2D1::Point2F(Rect().left, Rect().bottom),
        std::make_unique<DropdownPopupContent>(this));
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
