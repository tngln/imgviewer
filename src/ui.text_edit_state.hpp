#pragma once

#include <string>
#include <vector>

#include <d2d1_1.h>
#include <dwrite.h>
#include <wil/com.h>

class TextEditState final {
public:
    const std::wstring& Text() const;
    void SetText(std::wstring text);
    const std::wstring& Composition() const;
    void SetComposition(std::wstring composition);
    void ClearComposition();

    size_t Caret() const;
    size_t Anchor() const;
    bool HasSelection() const;
    size_t SelectionStart() const;
    size_t SelectionEnd() const;
    std::wstring SelectedText() const;
    std::wstring DisplayText() const;

    bool InsertCharacter(wchar_t ch);
    bool InsertText(const std::wstring& text);
    bool Backspace();
    bool Delete();
    bool SelectAll();
    bool MoveCaret(size_t index, bool extend_selection);
    bool MoveLeft(bool extend_selection);
    bool MoveRight(bool extend_selection);
    bool MoveHome(bool extend_selection);
    bool MoveEnd(bool extend_selection);

    size_t HitTest(IDWriteFactory* factory, IDWriteTextFormat* format, D2D1_POINT_2F local_point, float width, float layout_height) const;
    wil::com_ptr<IDWriteTextLayout> CreateLayout(
        IDWriteFactory* factory,
        IDWriteTextFormat* format,
        const std::wstring& value,
        float width,
        float layout_height) const;
    bool CaretMetrics(IDWriteTextLayout* layout, D2D1_POINT_2F origin, D2D1_POINT_2F* top, D2D1_POINT_2F* bottom) const;
    std::vector<DWRITE_HIT_TEST_METRICS> SelectionMetrics(IDWriteTextLayout* layout, D2D1_POINT_2F origin) const;

private:
    void DeleteSelection();

    std::wstring text_;
    std::wstring composition_;
    size_t caret_ = 0;
    size_t anchor_ = 0;
};
