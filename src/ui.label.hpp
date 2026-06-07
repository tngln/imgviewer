#pragma once

#include "ui.element.hpp"

enum class LabelStyle {
    Title,
    Body,
    Muted,
};

class Label final : public UiElement {
public:
    Label(UiElementMetadata metadata, const wchar_t* text, LabelStyle style = LabelStyle::Body);

    void SetText(const wchar_t* text);
    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const override;
    void Render(const UiDrawContext& context, UiRootState state) const override;

private:
    const wchar_t* text_ = L"";
    LabelStyle style_ = LabelStyle::Body;
};
