#pragma once

#include <d2d1_1.h>

#include <memory>
#include <vector>

#include "imgviewer.action.hpp"
#include "ui.draw.hpp"

struct UiEventResult;
struct UiInputEvent;
struct UiKeyEvent;
struct UiPointerEvent;

enum class UiElementId : int {
    None = 0,
};

constexpr int UiElementIdValue(UiElementId id)
{
    return static_cast<int>(id);
}

constexpr int UiElementRuntimeId(UiElementId id)
{
    return UiElementIdValue(id);
}

class UiElementIdGenerator final {
public:
    UiElementId Next();

private:
    int next_id_ = UiElementIdValue(UiElementId::None) + 1;
};

enum class UiElementRole {
    Button,
    CheckBox,
    ComboBox,
    Edit,
    Menu,
    MenuItem,
    RadioButton,
    Text,
    Pane,
};

struct UiElementMetadata final {
    UiElementId id = UiElementId::None;
    UiElementRole role = UiElementRole::Button;
    ImgViewerAction action = ImgViewerAction::None;
    const wchar_t* name = L"";
    const wchar_t* tooltip = L"";
    const wchar_t* automation_id = L"";
    bool is_control = true;
    bool is_content = true;
};

struct UiElementState final {
    bool hovered = false;
    bool pressed = false;
    bool active = false;
    bool danger = false;
    bool enabled = true;
    bool checked = false;
    bool expanded = false;
};

class UiElement {
public:
    explicit UiElement(UiElementMetadata metadata);
    virtual ~UiElement() = default;

    void SetRect(D2D1_RECT_F rect);
    D2D1_RECT_F Rect() const;
    const UiElementMetadata& Metadata() const;
    UiElementId Id() const;
    ImgViewerAction Action() const;
    void SetEnabled(bool enabled);
    bool IsEnabled() const;
    void SetFocusable(bool focusable);
    bool IsFocusable() const;
    void SetHitTestVisible(bool hit_test_visible);
    bool IsHitTestVisible() const;
    bool Contains(D2D1_POINT_2F point) const;
    virtual void Draw(const UiDrawContext& context, UiElementState state) const;
    virtual UiEventResult OnInputEvent(const UiInputEvent& event);
    virtual UiEventResult OnPointerEvent(const UiPointerEvent& event);
    virtual UiEventResult OnKeyEvent(const UiKeyEvent& event);
    UiElement* AddChild(std::unique_ptr<UiElement> child);
    size_t ChildCount() const;
    UiElement* ChildAt(size_t index);
    const UiElement* ChildAt(size_t index) const;
    UiElement* FindById(UiElementId id);
    const UiElement* FindById(UiElementId id) const;
    UiElement* HitTest(D2D1_POINT_2F point);
    const UiElement* HitTest(D2D1_POINT_2F point) const;

private:
    UiElementMetadata metadata_;
    D2D1_RECT_F rect_ = {};
    bool enabled_ = true;
    bool focusable_ = false;
    bool hit_test_visible_ = true;
    std::vector<std::unique_ptr<UiElement>> children_;
};
