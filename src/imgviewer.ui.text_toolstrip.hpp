#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <d2d1_1.h>

#include "imgviewer.edit.hpp"
#include "imgviewer.ui.floating_toolbar.hpp"
#include "ui.events.hpp"
#include "ui.selection.hpp"

struct ImgViewerUiTextToolstripState final {
    bool visible = false;
    ImgViewerTextStyle style;
};

class ImgViewerUiTextToolstrip final {
public:
    enum class ButtonKey : size_t {
        Size12,
        Size16,
        Size20,
        Size28,
        Size36,
        TextRed,
        TextYellow,
        TextGreen,
        TextCyan,
        TextBlue,
        TextMagenta,
        TextWhite,
        TextBlack,
        BackgroundTransparent,
        BackgroundYellow,
        BackgroundWhite,
        BackgroundBlack,
        BackgroundRed,
        BackgroundBlue,
        Count,
    };

    static constexpr size_t kButtonCount = static_cast<size_t>(ButtonKey::Count);
    static constexpr size_t ButtonIndex(ButtonKey button);

    explicit ImgViewerUiTextToolstrip(UiElement& root);

    void SetScalePercent(int percent);
    void SetState(ImgViewerUiTextToolstripState state);
    const std::wstring& SelectedFontFamily() const;
    D2D1_RECT_F Rect() const;
    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size);
    void Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F anchor_toolbar_rect);
    void Render(const UiDrawContext& draw_context, UiRootState state);
    UiEventResult OnPointerEvent(const UiPointerEvent& event);

private:
    struct ButtonInstance final {
        UiElementId id = UiElementId::None;
        UiElement* element = nullptr;
    };

    void EnsureFontOptions(IDWriteFactory* dwrite_factory);
    void SyncFontSelection();
    void UpdateVisualState();

    std::array<ButtonInstance, kButtonCount> buttons_{};
    Dropdown* font_dropdown_ = nullptr;
    std::unique_ptr<ImgViewerFloatingToolbar> toolbar_;
    ImgViewerUiTextToolstripState state_;
    std::vector<std::wstring> font_names_;
    std::vector<DropdownOption> font_options_;
    std::wstring selected_font_family_ = L"Segoe UI";
    bool fonts_loaded_ = false;
    int scale_percent_ = 125;
};
