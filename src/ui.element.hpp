#pragma once

#include <d2d1_1.h>

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

class UiElement {
public:
    explicit UiElement(UiElementMetadata metadata);

    void SetRect(D2D1_RECT_F rect);
    D2D1_RECT_F Rect() const;
    const UiElementMetadata& Metadata() const;
    UiElementId Id() const;
    UiCommand Command() const;
    bool Contains(D2D1_POINT_2F point) const;

protected:
    ~UiElement() = default;

private:
    UiElementMetadata metadata_;
    D2D1_RECT_F rect_ = {};
};
