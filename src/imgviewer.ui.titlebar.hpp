#pragma once

#include <array>
#include <cstddef>
#include <string>

#include <d2d1_1.h>
#include <dwrite.h>

#include "ui.button.hpp"
#include "ui.element.hpp"

struct ImageViewerUiState;

class ImageViewerUiTitleBar final {
public:
    enum class ButtonKey : size_t {
        TopMost,
        Minimize,
        MaximizeRestore,
        Close,
        Count,
    };

    static constexpr size_t kButtonCount = static_cast<size_t>(ButtonKey::Count);
    static constexpr size_t ButtonIndex(ButtonKey button);

    ImageViewerUiTitleBar(UiElement& root, UiElementIdGenerator& ids);

    void Draw(
        const UiDrawContext& draw_context,
        D2D1_SIZE_F viewport_size,
        IDWriteFactory* dwrite_factory,
        IDWriteTextFormat* body_text_format,
        ImageViewerUiState state,
        bool top_most,
        bool maximized);
    bool IsPointInCaptionDragArea(const UiElement& root, D2D1_POINT_2F point) const;
    void SetTitleText(const wchar_t* title);

private:
    struct ButtonInstance final {
        UiElementId id = UiElementId::None;
        IconButton* element = nullptr;
    };

    void Layout(D2D1_SIZE_F viewport_size);
    IconButton* Button(ButtonKey button);
    const IconButton* Button(ButtonKey button) const;
    UiElementState ButtonState(ButtonKey button, ImageViewerUiState state, bool active = false, bool danger = false) const;
    void DrawButton(
        ButtonKey button,
        const UiDrawContext& draw_context,
        ImageViewerUiState state,
        bool active = false,
        bool danger = false) const;

    std::array<ButtonInstance, kButtonCount> buttons_{};
    std::wstring title_text_ = L"ImgViewer";
    D2D1_RECT_F titlebar_rect_ = D2D1_RECT_F{0.0f, 0.0f, 960.0f, 48.0f};
    D2D1_RECT_F title_text_rect_ = D2D1_RECT_F{16.0f, 0.0f, 720.0f, 48.0f};
};
