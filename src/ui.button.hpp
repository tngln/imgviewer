#pragma once

#include <cstddef>

#include <dwrite.h>

#include "ui.element.hpp"

class Button final : public UiElement {
public:
    Button(UiElementMetadata metadata, const wchar_t* icon, const wchar_t* text);

    float PreferredWidth(IDWriteFactory* factory, IDWriteTextFormat* body_text_format) const;
    void Draw(const UiDrawContext& context, UiElementState state) const override;

private:
    const wchar_t* icon_ = L"";
    const wchar_t* text_ = L"";
};

class IconButton final : public UiElement {
public:
    IconButton(UiElementMetadata metadata, const wchar_t* icon);
    IconButton(
        UiElementMetadata metadata,
        const icons::PathCommand* icon_path,
        size_t icon_path_count,
        float icon_viewport);

    void SetIcon(const wchar_t* icon);
    void Draw(const UiDrawContext& context, UiElementState state) const override;

private:
    const wchar_t* icon_ = L"";
    const icons::PathCommand* icon_path_ = nullptr;
    size_t icon_path_count_ = 0;
    float icon_viewport_ = 0.0f;
};
