#include "ui.label.hpp"

#include <algorithm>
#include <cwchar>

#include <d2d1helper.h>

#include "ui.theme.hpp"

namespace {

constexpr float kLabelTitleHeight = 17.0f;
constexpr float kLabelBodyHeight = 13.0f;

float LabelHeight(LabelStyle style)
{
    return style == LabelStyle::Title ? kLabelTitleHeight : kLabelBodyHeight;
}

D2D1_COLOR_F LabelColor(LabelStyle style)
{
    return style == LabelStyle::Muted ? ui_theme::color::kMutedText : ui_theme::color::kBodyText;
}

} // namespace

Label::Label(UiElementMetadata metadata, const wchar_t* text, LabelStyle style) :
    UiElement(metadata),
    text_(text),
    style_(style)
{
}

void Label::SetText(const wchar_t* text)
{
    text_ = text;
}

D2D1_SIZE_F Label::Measure(const UiDrawContext&, D2D1_SIZE_F available_size) const
{
    return D2D1::SizeF((std::max)(1.0f, available_size.width), LabelHeight(style_));
}

void Label::Render(const UiDrawContext& context, UiRootState) const
{
    const UiDraw draw(context);
    draw.DrawBodyText(
        text_,
        static_cast<UINT32>(wcslen(text_)),
        Rect(),
        LabelColor(style_),
        D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
}
