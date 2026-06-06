#pragma once

#include <string>

#include <d2d1_1.h>
#include <dwrite.h>

#include "ui.draw.hpp"

class UiToast final {
public:
    void Render(const UiDrawContext& draw_context) const;
    void Show(const wchar_t* text);
    bool Hide();
    bool IsVisible() const;

private:
    std::wstring text_;
};
