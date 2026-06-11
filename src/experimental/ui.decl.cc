#include "experimental/ui.decl.hpp"

#include <d2d1helper.h>

#include "ui.draw.hpp"
#include "ui.theme.hpp"

namespace experimental::ui_decl {
namespace {

constexpr float kSectionGap = 10.0f;
constexpr float kSectionTopPadding = 10.0f;

} // namespace

BorderedStack::BorderedStack(UiElementMetadata metadata) : UiElement(metadata)
{
    panel_ = static_cast<StackPanel*>(AddChild(std::make_unique<StackPanel>(PaneMetadata())));
}

void BorderedStack::SetGap(float gap)
{
    panel_->SetGap(gap);
}

void BorderedStack::SetPadding(UiThickness padding)
{
    panel_->SetPadding(padding);
}

D2D1_SIZE_F BorderedStack::Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const
{
    return panel_->Measure(context, available_size);
}

void BorderedStack::Arrange(D2D1_RECT_F final_rect)
{
    UiElement::Arrange(final_rect);
    panel_->Arrange(final_rect);
}

void BorderedStack::Render(const UiDrawContext& context, UiRootState state) const
{
    const UiDraw draw(context);
    draw.DrawRect(Rect(), ui_theme::color::kBorder);
    panel_->Render(context, state);
}

UiElementMetadata PaneMetadata(const wchar_t* automation_id)
{
    return UiMetadata(UiElementRole::Pane, kUiActionNone, L"", L"", automation_id, false, false);
}

UiElementMetadata TextMetadata(const wchar_t* text, const wchar_t* automation_id)
{
    return UiMetadata(UiElementRole::Text, kUiActionNone, text, L"", automation_id, false, false);
}

std::unique_ptr<Label> Title(const wchar_t* text, const wchar_t* automation_id)
{
    return std::make_unique<Label>(TextMetadata(text, automation_id), text, LabelStyle::Title);
}

std::unique_ptr<Label> Body(const wchar_t* text, const wchar_t* automation_id)
{
    return std::make_unique<Label>(TextMetadata(text, automation_id), text, LabelStyle::Body);
}

std::unique_ptr<Label> Muted(const wchar_t* text, const wchar_t* automation_id)
{
    return std::make_unique<Label>(TextMetadata(text, automation_id), text, LabelStyle::Muted);
}

std::unique_ptr<::Button> ActionButton(
    UiAction action,
    const wchar_t* name,
    const wchar_t* automation_id,
    const wchar_t* icon,
    const wchar_t* text)
{
    return std::make_unique<::Button>(
        UiMetadata(UiElementRole::Button, action, name, name, automation_id),
        icon,
        text);
}

std::unique_ptr<::Button> Button(
    const wchar_t* name,
    const wchar_t* automation_id,
    const wchar_t* icon,
    const wchar_t* text)
{
    return std::make_unique<::Button>(
        UiMetadata(UiElementRole::Button, kUiActionNone, name, name, automation_id),
        icon,
        text);
}

std::unique_ptr<::Checkbox> Toggle(
    const wchar_t* text,
    const wchar_t* automation_id,
    bool checked)
{
    return std::make_unique<::Checkbox>(
        UiMetadata(UiElementRole::CheckBox, kUiActionNone, text, text, automation_id),
        text,
        checked);
}

std::unique_ptr<::SliderRow> SliderField(
    const wchar_t* name,
    const wchar_t* automation_id,
    int minimum,
    int maximum,
    int value,
    int small_step,
    int large_step)
{
    return std::make_unique<::SliderRow>(
        UiMetadata(UiElementRole::Slider, kUiActionNone, name, name, automation_id),
        minimum,
        maximum,
        value,
        small_step,
        large_step);
}

std::unique_ptr<StackPanel> Section(
    const wchar_t* title,
    std::unique_ptr<UiElement> content,
    const wchar_t* title_automation_id,
    const wchar_t* section_automation_id)
{
    auto section = std::make_unique<StackPanel>(PaneMetadata(section_automation_id));
    section->SetPadding(UiThickness{0.0f, kSectionTopPadding, 0.0f, 0.0f});
    section->SetGap(kSectionGap);
    section->AddItem(Body(title, title_automation_id));
    section->AddItem(std::move(content));
    return section;
}

} // namespace experimental::ui_decl
