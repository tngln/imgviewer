#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "ui.element.hpp"

class Dropdown;
class TextBox;

enum class TableCellEditor {
    Text,
    Dropdown,
};

struct TableColumn final {
    const wchar_t* header = L"";
    float width = 0.0f;
    bool fill = false;
    bool editable = false;
    TableCellEditor editor = TableCellEditor::Text;
    std::vector<std::wstring> dropdown_options;
};

struct TableRow final {
    std::vector<std::wstring> cells;
    UiAction action = kUiActionNone;
    bool section = false;
    bool enabled = true;
};

struct TableEditCommit final {
    size_t row = 0;
    size_t column = 0;
    std::wstring value;
};

class Table final : public UiElement {
public:
    explicit Table(UiElementMetadata metadata);

    void SetColumns(std::vector<TableColumn> columns);
    void SetRows(std::vector<TableRow> rows);
    void SetHeaderVisible(bool visible);
    void SetSelectionEnabled(bool enabled);
    void SetRowHeight(float row_height);
    void SetCellPadding(float padding);
    void SetSeparatorsVisible(bool visible);
    size_t SelectedIndex() const;
    UiAction SelectedAction() const;
    void SetSelectedIndex(size_t index);
    bool IsEditing() const;
    UiElementId EditorId() const;
    bool IsEditorElement(UiElementId id) const;
    std::optional<TableEditCommit> TakeEditCommit();
    UiEventResult CommitEdit();
    UiEventResult ExecuteEditAction(UiAction action, HWND hwnd);

    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const override;
    void Arrange(D2D1_RECT_F final_rect) override;
    void Render(const UiDrawContext& context, UiRootState state) const override;
    UiEventResult OnInputEvent(const UiInputEvent& event) override;
    UiEventResult OnPointerEvent(const UiPointerEvent& event) override;
    UiEventResult OnKeyEvent(const UiKeyEvent& event) override;

private:
    static constexpr size_t kNoSelection = static_cast<size_t>(-1);

    D2D1_RECT_F HeaderRect() const;
    D2D1_RECT_F BodyRect() const;
    D2D1_RECT_F RowRect(size_t index) const;
    D2D1_RECT_F CellRect(D2D1_RECT_F row_rect, size_t column_index, const std::vector<float>& widths) const;
    size_t RowAt(D2D1_POINT_2F point) const;
    size_t ColumnAt(D2D1_POINT_2F point) const;
    bool IsSelectableRow(size_t index) const;
    bool IsEditableCell(size_t row, size_t column) const;
    size_t NextSelectable(size_t start) const;
    size_t PreviousSelectable(size_t start) const;
    std::vector<float> ColumnWidths(float total_width) const;
    float BodyContentHeight() const;
    float BodyViewportHeight() const;
    float MaxScrollOffset() const;
    float EffectiveScrollOffset() const;
    void EnsureSelectionVisible();
    void ArrangeEditor();
    UiElement* ActiveEditor();
    const UiElement* ActiveEditor() const;
    std::wstring ActiveEditorValue() const;
    UiEventResult BeginEdit(size_t row, size_t column);
    UiEventResult BeginEditSelectedCell();
    UiEventResult EndEdit(bool commit);

    std::vector<TableColumn> columns_;
    std::vector<TableRow> rows_;
    TextBox* text_editor_ = nullptr;
    Dropdown* dropdown_editor_ = nullptr;
    bool editor_active_ = false;
    TableCellEditor active_editor_ = TableCellEditor::Text;
    size_t edit_row_ = kNoSelection;
    size_t edit_column_ = kNoSelection;
    std::optional<TableEditCommit> last_edit_commit_;
    size_t selected_index_ = kNoSelection;
    size_t hovered_index_ = kNoSelection;
    bool header_visible_ = false;
    bool selection_enabled_ = false;
    bool separators_visible_ = true;
    float row_height_ = 21.0f;
    float cell_padding_ = 6.0f;
    float scroll_offset_ = 0.0f;
};
