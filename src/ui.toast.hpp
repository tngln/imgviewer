#pragma once

#include <string>

#include <d2d1_1.h>
#include <dwrite.h>

#include "ui.draw.hpp"

class UiToast final {
public:
    void Draw(
        const UiDrawContext& draw_context,
        D2D1_SIZE_F viewport_size,
        IDWriteFactory* dwrite_factory,
        IDWriteTextFormat* body_text_format) const;
    void Show(const wchar_t* text);
    bool Hide();
    bool IsVisible() const;

private:
    std::wstring text_;
};
