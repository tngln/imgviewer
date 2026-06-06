#include "ui.hpp"

#include <memory>

#include <d2d1helper.h>

UiController::UiController(std::unique_ptr<UiRoot> root) : root_(std::move(root)) {}

UiController::~UiController() = default;

void UiController::ResetRoot(std::unique_ptr<UiRoot> root)
{
    root_ = std::move(root);
    hovered_id_ = UiElementId::None;
    pressed_id_ = UiElementId::None;
    focused_id_ = UiElementId::None;
    captured_id_ = UiElementId::None;
}

UiRoot* UiController::Root()
{
    return root_.get();
}

const UiRoot* UiController::Root() const
{
    return root_.get();
}

UiEventResult UiController::OnInputEvent(const UiInputEvent& event)
{
    switch (event.type) {
    case UiEventType::PointerMove:
    case UiEventType::PointerDown:
    case UiEventType::PointerUp:
    case UiEventType::PointerLeave:
    case UiEventType::PointerWheel:
        return OnPointerEvent(event.pointer);
    case UiEventType::KeyDown:
    case UiEventType::KeyUp: {
        UiKeyEvent key = event.key;
        if (key.focused == UiElementId::None) {
            key.focused = focused_id_;
        }
        return OnKeyEvent(key);
    }
    case UiEventType::Cancel:
        return OnKeyEvent(UiKeyEvent{.type = UiEventType::KeyDown, .virtual_key = VK_ESCAPE, .focused = focused_id_});
    case UiEventType::OwnerDeactivated:
        root_->OnKeyEvent(UiKeyEvent{.type = UiEventType::KeyDown, .virtual_key = VK_ESCAPE, .focused = focused_id_});
        return UiEventResult{.handled = true, .needs_render = true};
    case UiEventType::ContextMenu: {
        UiInputEvent root_event = event;
        root_event.focused = focused_id_;
        UiEventResult result = root_->OnInputEvent(root_event);
        if (!result.handled) {
            const UiElementId target_id = HitTest(event.point);
            if (UiElement* target = root_->Root()->FindById(target_id)) {
                UiInputEvent target_event = event;
                target_event.focused = focused_id_;
                result = target->OnInputEvent(target_event);
                ApplyEventResult(result, result.focus_target != UiElementId::None ? result.focus_target : target_id);
                return result;
            }
        }
        ApplyEventResult(result, result.focus_target != UiElementId::None ? result.focus_target : focused_id_);
        return result;
    }
    default:
        UiInputEvent root_event = event;
        root_event.focused = focused_id_;
        UiEventResult result = root_->OnInputEvent(root_event);
        ApplyEventResult(result, result.focus_target != UiElementId::None ? result.focus_target : focused_id_);
        return result;
    }
}

UiEventResult UiController::OnPointerEvent(const UiPointerEvent& event)
{
    UiEventResult result = DispatchPointerEvent(event);
    if (root_->HandleUiAction(result.action)) {
        result.action = kUiActionNone;
        result.handled = true;
        result.needs_render = true;
    }
    const UiElementId target = event.captured != UiElementId::None ? event.captured : result.focus_target;
    ApplyEventResult(result, target != UiElementId::None ? target : event.target);
    return result;
}

UiEventResult UiController::OnKeyEvent(const UiKeyEvent& event)
{
    UiEventResult menu_result = root_->OnKeyEvent(event);
    if (menu_result.handled) {
        return menu_result;
    }
    UiEventResult result = DispatchKeyEvent(event);
    ApplyEventResult(result, event.focused);
    if (result.handled && event.focused != UiElementId::None) {
        root_->ApplyElementEffect(event.focused);
    }
    return result;
}

UiElementId UiController::HoveredElement() const
{
    return hovered_id_;
}

UiElementId UiController::PressedElement() const
{
    return pressed_id_;
}

UiElementId UiController::FocusedElement() const
{
    return focused_id_;
}

UiElementId UiController::CapturedElement() const
{
    return captured_id_;
}

UiEventResult UiController::DispatchPointerEvent(const UiPointerEvent& event)
{
    if (event.type == UiEventType::PointerLeave) {
        const bool had_hover = hovered_id_ != UiElementId::None;
        hovered_id_ = UiElementId::None;
        return UiEventResult{
            .handled = had_hover || captured_id_ != UiElementId::None,
            .needs_render = had_hover,
        };
    }

    const UiElementId hit_id = HitTest(event.point);
    const UiElementId target_id = captured_id_ != UiElementId::None ? captured_id_ : hit_id;
    const UiElementId was_hovered = hovered_id_;
    hovered_id_ = hit_id;

    UiPointerEvent target_event = event;
    target_event.target = hit_id;
    target_event.captured = captured_id_;

    UiEventResult root_result = root_->OnPointerEvent(target_event);
    if (root_result.handled) {
        root_result.needs_render = root_result.needs_render || was_hovered != hovered_id_;
        return root_result;
    }

    UiEventResult result = root_result;
    if (UiElement* target = root_->Root()->FindById(target_id)) {
        result = target->OnInputEvent(UiInputEvent{.type = target_event.type, .pointer = target_event, .point = target_event.point});
        result.needs_render = result.needs_render || root_result.needs_render;
    }

    if (!result.handled && target_id != UiElementId::None) {
        result.handled = true;
    }
    result.needs_render = result.needs_render || was_hovered != hovered_id_;

    if (event.type == UiEventType::PointerDown && result.capture == UiCaptureRequest::Capture) {
        result.focus_target = target_id;
    }

    if (event.type == UiEventType::PointerUp && target_id != UiElementId::None && hit_id == target_id) {
        root_->ApplyElementEffect(target_id);
    } else if (result.value_changed && target_id != UiElementId::None) {
        root_->ApplyElementEffect(target_id);
    }

    return result;
}

UiEventResult UiController::DispatchKeyEvent(const UiKeyEvent& event)
{
    if (focused_id_ == UiElementId::None) {
        return {};
    }

    if (UiElement* focused = root_->Root()->FindById(focused_id_)) {
        UiKeyEvent focused_event = event;
        focused_event.focused = focused_id_;
        return focused->OnInputEvent(UiInputEvent{.type = focused_event.type, .key = focused_event});
    }

    return {};
}

void UiController::ApplyEventResult(const UiEventResult& result, UiElementId target)
{
    if (result.capture == UiCaptureRequest::Capture) {
        captured_id_ = target;
        pressed_id_ = target;
    } else if (result.capture == UiCaptureRequest::Release) {
        captured_id_ = UiElementId::None;
        pressed_id_ = UiElementId::None;
    }

    if (result.focus == UiFocusRequest::FocusTarget) {
        focused_id_ = result.focus_target != UiElementId::None ? result.focus_target : target;
    } else if (result.focus == UiFocusRequest::ClearFocus) {
        focused_id_ = UiElementId::None;
    }
}

void UiController::Draw(const UiDrawContext& context)
{
    root_->Draw(
        context,
        UiRootState{
            .hovered = hovered_id_,
            .pressed = pressed_id_,
            .focused = focused_id_,
        });
}

const wchar_t* UiController::AccessibilityRootName() const
{
    return root_->AccessibilityRootName();
}

UiElementId UiController::HitTest(D2D1_POINT_2F point) const
{
    return root_->HitTest(point);
}

const UiElementMetadata* UiController::MetadataForElement(UiElementId id) const
{
    const UiElement* ui_root = root_->Root();
    const UiElement* element = ui_root->FindById(id);
    return element != nullptr && element != ui_root ? &element->Metadata() : nullptr;
}

size_t UiController::ElementCount() const
{
    return root_->Root()->ChildCount();
}

const UiElementMetadata* UiController::ElementMetadataAt(size_t index) const
{
    const UiElement* element = root_->Root()->ChildAt(index);
    return element != nullptr ? &element->Metadata() : nullptr;
}

const UiElementMetadata* UiController::ElementMetadata(UiElementId id) const
{
    return MetadataForElement(id);
}

D2D1_RECT_F UiController::ElementRect(UiElementId id) const
{
    const UiElement* ui_root = root_->Root();
    const UiElement* element = ui_root->FindById(id);
    return element != nullptr && element != ui_root ? element->Rect() : D2D1::RectF();
}

bool UiController::IsElementEnabled(UiElementId id) const
{
    const UiElement* ui_root = root_->Root();
    const UiElement* element = ui_root->FindById(id);
    return element != nullptr && element != ui_root && element->IsEnabled();
}

bool UiController::IsPointInCaptionDragArea(D2D1_POINT_2F point) const
{
    return root_->IsPointInCaptionDragArea(point);
}

void UiController::SetTitleText(const wchar_t* title)
{
    root_->SetTitleText(title);
}

void UiController::ShowToast(const wchar_t* text)
{
    root_->ShowToast(text);
}

bool UiController::HideToast()
{
    return root_->HideToast();
}

void UiController::SetActionEnabled(UiAction action, bool enabled)
{
    if (action == kUiActionNone) {
        return;
    }

    root_->SetActionEnabled(action, enabled);
    SetActionEnabledRecursive(root_->Root(), action, enabled);
}

const wchar_t* UiController::ElementValue(UiElementId id) const
{
    return root_->ElementValue(id);
}

double UiController::ElementRangeValue(UiElementId id) const
{
    return root_->ElementRangeValue(id);
}

double UiController::ElementRangeMinimum(UiElementId id) const
{
    return root_->ElementRangeMinimum(id);
}

double UiController::ElementRangeMaximum(UiElementId id) const
{
    return root_->ElementRangeMaximum(id);
}

double UiController::ElementRangeSmallChange(UiElementId id) const
{
    return root_->ElementRangeSmallChange(id);
}

double UiController::ElementRangeLargeChange(UiElementId id) const
{
    return root_->ElementRangeLargeChange(id);
}

HRESULT UiController::SetElementRangeValue(UiElementId id, double value)
{
    return root_->SetElementRangeValue(id, value);
}

void UiController::SetWindowState(bool top_most, bool maximized)
{
    root_->SetWindowState(top_most, maximized);
}

void UiController::SetColorPickerActive(bool active)
{
    root_->SetColorPickerActive(active);
}

void UiController::SetToolbarScalePercent(int percent)
{
    root_->SetToolbarScalePercent(percent);
}

void UiController::SetActionEnabledRecursive(UiElement* element, UiAction action, bool enabled)
{
    if (element == nullptr) {
        return;
    }

    if (element->Action() == action) {
        element->SetEnabled(enabled);
    }

    for (size_t index = 0; index < element->ChildCount(); ++index) {
        SetActionEnabledRecursive(element->ChildAt(index), action, enabled);
    }
}
