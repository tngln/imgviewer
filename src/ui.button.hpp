#pragma once

#include <cstddef>

#include <dwrite.h>

#include "ui.element.hpp"
#include "ui.events.hpp"

class Button final : public UiElement {
public:
    Button(UiElementMetadata metadata, const wchar_t* icon, const wchar_t* text);

    float PreferredWidth(IDWriteFactory* factory, IDWriteTextFormat* body_text_format) const;
    void Draw(const UiDrawContext& context, UiElementState state) const override;
    UiEventResult OnPointerEvent(const UiPointerEvent& event) override;
    UiEventResult OnKeyEvent(const UiKeyEvent& event) override;

private:
    const wchar_t* icon_ = L"";
    const wchar_t* text_ = L"";
};

class IconButton final : public UiElement {
public:
    IconButton(UiElementMetadata metadata, const wchar_t* icon);
    IconButton(
        UiElementMetadata metadata,
        const icons::PathIcon& icon);

    void SetIcon(const wchar_t* icon);
    void Draw(const UiDrawContext& context, UiElementState state) const override;
    UiEventResult OnPointerEvent(const UiPointerEvent& event) override;
    UiEventResult OnKeyEvent(const UiKeyEvent& event) override;

private:
    const wchar_t* icon_ = L"";
    const icons::PathIcon* path_icon_ = nullptr;
};
