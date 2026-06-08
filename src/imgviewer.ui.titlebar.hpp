#pragma once

#include <array>
#include <cstddef>
#include <string>

#include <d2d1_1.h>
#include <dwrite.h>

#include "ui.button.hpp"
#include "ui.element.hpp"
#include "ui.panel.hpp"

#include "ui.root.hpp"

class ImgViewerUiTitleBar final {
public:
    enum class ButtonKey : size_t {
        Menu,
        TopMost,
        Minimize,
        MaximizeRestore,
        Close,
        Count,
    };

    static constexpr size_t kButtonCount = static_cast<size_t>(ButtonKey::Count);
    static constexpr size_t ButtonIndex(ButtonKey button);

    explicit ImgViewerUiTitleBar(UiElement& root);

    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const;
    void Arrange(D2D1_RECT_F final_rect);
    void Render(
        const UiDrawContext& draw_context,
        UiRootState state,
        bool top_most,
        bool maximized,
        bool edit_mode);
    bool IsPointInCaptionDragArea(const UiElement& root, D2D1_POINT_2F point) const;
    void SetTitleText(const wchar_t* title);

private:
    struct ButtonInstance final {
        UiElementId id = UiElementId::None;
        IconButton* element = nullptr;
    };

    IconButton* Button(ButtonKey button);
    const IconButton* Button(ButtonKey button) const;
    void RenderButton(
        ButtonKey button,
        const UiDrawContext& draw_context,
        UiRootState state,
        bool danger = false);

    std::array<ButtonInstance, kButtonCount> buttons_{};
    StackPanel* caption_buttons_ = nullptr;
    std::wstring title_text_ = L"ImgViewer";
    D2D1_RECT_F titlebar_rect_ = D2D1_RECT_F{0.0f, 0.0f, 960.0f, 48.0f};
    D2D1_RECT_F title_text_rect_ = D2D1_RECT_F{16.0f, 0.0f, 720.0f, 48.0f};
};
