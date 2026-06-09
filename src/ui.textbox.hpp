#pragma once

#include <string>

#include <dwrite.h>
#include <wil/com.h>

#include "ui.element.hpp"
#include "ui.events.hpp"
#include "ui.menu.hpp"
#include "ui.text_edit_state.hpp"

class TextBox final : public UiElement {
public:
    TextBox(UiElementMetadata metadata, const wchar_t* placeholder);

    const std::wstring& Text() const;
    void SetText(std::wstring text);
    void SelectAll();
    void SetTextServices(IDWriteFactory* factory, IDWriteTextFormat* format) const;
    void SetCaretVisible(bool visible);
    bool IsEditing() const;
    D2D1_POINT_2F CaretPoint() const;
    std::vector<MenuItem> ContextMenuItems() const;

    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const override;
    void Render(const UiDrawContext& context, UiRootState state) const override;
    UiEventResult OnInputEvent(const UiInputEvent& event) override;
    UiEventResult OnPointerEvent(const UiPointerEvent& event) override;
    UiEventResult OnKeyEvent(const UiKeyEvent& event) override;
    UiEventResult ExecuteEditAction(UiAction action, HWND hwnd);

private:
    UiEventResult InsertCharacter(wchar_t ch);
    UiEventResult UpdateImeComposition(std::wstring composition);
    UiEventResult EndImeComposition();
    size_t HitTest(D2D1_POINT_2F point) const;
    wil::com_ptr<IDWriteTextLayout> CreateLayout(const std::wstring& value, float width) const;
    void UpdateHorizontalScroll();
    bool CopySelection(HWND hwnd) const;
    bool PasteClipboard(HWND hwnd);

    const wchar_t* placeholder_ = L"";
    TextEditState edit_;
    float horizontal_scroll_ = 0.0f;
    mutable bool caret_visible_ = true;
    bool dragging_ = false;
    mutable D2D1_POINT_2F caret_point_ = {};
    mutable IDWriteFactory* dwrite_factory_ = nullptr;
    mutable IDWriteTextFormat* text_format_ = nullptr;
};
