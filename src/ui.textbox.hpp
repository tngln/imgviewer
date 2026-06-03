#pragma once

#include <string>

#include <dwrite.h>
#include <wil/com.h>

#include "ui.element.hpp"
#include "ui.events.hpp"
#include "ui.menu.hpp"

class TextBox final : public UiElement {
public:
    TextBox(UiElementMetadata metadata, const wchar_t* placeholder);

    const std::wstring& Text() const;
    void SetText(std::wstring text);
    void SetTextServices(IDWriteFactory* factory, IDWriteTextFormat* format);
    void SetCaretVisible(bool visible);
    bool IsEditing() const;
    bool IsContextMenuOpen() const;
    D2D1_POINT_2F CaretPoint() const;

    void Draw(const UiDrawContext& context, UiElementState state) const override;
    UiEventResult OnPointerEvent(const UiPointerEvent& event) override;
    UiEventResult OnKeyEvent(const UiKeyEvent& event) override;
    UiEventResult OnChar(wchar_t ch);
    UiEventResult OnImeComposition(std::wstring composition);
    UiEventResult EndImeComposition();
    UiEventResult OpenContextMenu(D2D1_POINT_2F point, HWND hwnd);
    UiEventResult ExecuteEditAction(ImgViewerAction action, HWND hwnd);

private:
    bool HasSelection() const;
    size_t SelectionStart() const;
    size_t SelectionEnd() const;
    void DeleteSelection();
    void InsertText(const std::wstring& text);
    void MoveCaret(size_t index, bool extend_selection);
    size_t HitTest(D2D1_POINT_2F point) const;
    wil::com_ptr<IDWriteTextLayout> CreateLayout(const std::wstring& value, float width) const;
    std::wstring DisplayText() const;
    void UpdateHorizontalScroll();
    std::wstring SelectedText() const;
    bool CopySelection(HWND hwnd) const;
    bool PasteClipboard(HWND hwnd);

    const wchar_t* placeholder_ = L"";
    std::wstring text_;
    std::wstring composition_;
    size_t caret_ = 0;
    size_t anchor_ = 0;
    float horizontal_scroll_ = 0.0f;
    bool caret_visible_ = true;
    bool dragging_ = false;
    D2D1_POINT_2F caret_point_ = {};
    IDWriteFactory* dwrite_factory_ = nullptr;
    IDWriteTextFormat* text_format_ = nullptr;
    MenuOverlay context_menu_;
};
