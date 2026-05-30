#pragma once

#include <d2d1_1.h>
#include <dwrite.h>

#include <memory>
#include <vector>

enum class UiCommand {
    None,
    OpenImage,
    ToggleTopMost,
    Minimize,
    ToggleMaximize,
    Close,
};

enum class UiElementId {
    None,
    OpenImage,
    Test,
    TopMost,
    Minimize,
    MaximizeRestore,
    Close,
};

enum class UiElementRole {
    Button,
    Text,
    Pane,
};

struct UiElementMetadata final {
    UiElementId id = UiElementId::None;
    UiElementRole role = UiElementRole::Button;
    UiCommand command = UiCommand::None;
    const wchar_t* name = L"";
    const wchar_t* automation_id = L"";
    int runtime_id = 0;
    bool is_control = true;
    bool is_content = true;
};

struct UiElementState final {
    bool hovered = false;
    bool pressed = false;
    bool active = false;
    bool danger = false;
};

struct UiDrawContext final {
    ID2D1DeviceContext* d2d_context = nullptr;
    IDWriteTextFormat* body_text_format = nullptr;
    IDWriteTextFormat* icon_text_format = nullptr;
};

class UiElement {
public:
    explicit UiElement(UiElementMetadata metadata);
    virtual ~UiElement() = default;

    void SetRect(D2D1_RECT_F rect);
    D2D1_RECT_F Rect() const;
    const UiElementMetadata& Metadata() const;
    UiElementId Id() const;
    UiCommand Command() const;
    bool Contains(D2D1_POINT_2F point) const;
    virtual void Draw(const UiDrawContext& context, UiElementState state) const;
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
    std::vector<std::unique_ptr<UiElement>> children_;
};
