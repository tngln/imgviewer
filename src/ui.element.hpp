#pragma once

#include <d2d1_1.h>

#include <memory>
#include <vector>

#include "ui.action.hpp"
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
    Slider,
    Text,
    Pane,
};

struct UiElementMetadata final {
    UiElementId id = UiElementId::None;
    UiElementRole role = UiElementRole::Button;
    UiAction action = kUiActionNone;
    const wchar_t* name = L"";
    const wchar_t* tooltip = L"";
    const wchar_t* automation_id = L"";
    bool is_control = true;
    bool is_content = true;
};

UiElementMetadata UiMetadata(
    UiElementRole role,
    UiAction action,
    const wchar_t* name,
    const wchar_t* tooltip,
    const wchar_t* automation_id,
    bool is_control = true,
    bool is_content = true);
UiElementMetadata UiRootMetadata(
    UiElementRole role,
    UiAction action,
    const wchar_t* name,
    const wchar_t* tooltip,
    const wchar_t* automation_id,
    bool is_control = true,
    bool is_content = true);

struct UiElementState final {
    bool hovered = false;
    bool pressed = false;
    bool active = false;
    bool danger = false;
    bool enabled = true;
    bool checked = false;
    bool expanded = false;
};

struct UiRootState final {
    UiElementId hovered = UiElementId::None;
    UiElementId pressed = UiElementId::None;
    UiElementId focused = UiElementId::None;
};

class UiElement {
public:
    explicit UiElement(UiElementMetadata metadata);
    virtual ~UiElement() = default;

    D2D1_RECT_F Rect() const;
    virtual D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const;
    virtual void Arrange(D2D1_RECT_F final_rect);
    const UiElementMetadata& Metadata() const;
    UiElementId Id() const;
    UiAction Action() const;
    void SetEnabled(bool enabled);
    bool IsEnabled() const;
    void SetFocusable(bool focusable);
    bool IsFocusable() const;
    void SetHitTestVisible(bool hit_test_visible);
    bool IsHitTestVisible() const;
    void SetVisualActive(bool active);
    bool IsVisualActive() const;
    void SetVisualDanger(bool danger);
    bool IsVisualDanger() const;
    UiElementState VisualState(UiRootState state) const;
    bool Contains(D2D1_POINT_2F point) const;
    virtual void Render(const UiDrawContext& context, UiRootState state) const;
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
    bool visual_active_ = false;
    bool visual_danger_ = false;
    std::vector<std::unique_ptr<UiElement>> children_;
};

class UiValueAccessible {
public:
    virtual ~UiValueAccessible() = default;

    virtual const wchar_t* AccessibilityValue() const = 0;
    virtual bool AccessibilityIsReadOnly() const { return false; }
};

class UiRangeAccessible {
public:
    virtual ~UiRangeAccessible() = default;

    virtual double AccessibilityRangeValue() const = 0;
    virtual double AccessibilityRangeMinimum() const = 0;
    virtual double AccessibilityRangeMaximum() const = 0;
    virtual double AccessibilityRangeSmallChange() const { return 1.0; }
    virtual double AccessibilityRangeLargeChange() const { return 10.0; }
    virtual HRESULT AccessibilitySetRangeValue(double) { return E_NOTIMPL; }
};
