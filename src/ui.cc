#include "ui.hpp"

#include <algorithm>
#include <memory>
#include <vector>

#include <d2d1helper.h>

#include "image.viewer.ui.hpp"

UiController::UiController() : app_ui_(std::make_unique<ImageViewerUi>())
{
    SetActionEnabled(AppAction::PreviousImage, false);
    SetActionEnabled(AppAction::NextImage, false);
}

UiController::~UiController() = default;

UiEventResult UiController::OnPointerMove(D2D1_POINT_2F point)
{
    return OnPointerEvent(UiPointerEvent{
        .type = UiEventType::PointerMove,
        .point = point,
    });
}

UiEventResult UiController::OnPointerDown(D2D1_POINT_2F point)
{
    return OnPointerEvent(UiPointerEvent{
        .type = UiEventType::PointerDown,
        .point = point,
        .button = UiPointerButton::Left,
    });
}

UiEventResult UiController::OnPointerUp(D2D1_POINT_2F point)
{
    return OnPointerEvent(UiPointerEvent{
        .type = UiEventType::PointerUp,
        .point = point,
        .button = UiPointerButton::Left,
    });
}

UiEventResult UiController::OnPointerLeave()
{
    return OnPointerEvent(UiPointerEvent{
        .type = UiEventType::PointerLeave,
    });
}

UiEventResult UiController::OnPointerEvent(const UiPointerEvent& event)
{
    UiEventResult result = DispatchPointerEvent(event);
    const UiElementId target = event.captured != UiElementId::None ? event.captured : result.focus_target;
    ApplyEventResult(result, target != UiElementId::None ? target : event.target);
    return result;
}

UiEventResult UiController::OnKeyEvent(const UiKeyEvent& event)
{
    UiEventResult result = DispatchKeyEvent(event);
    ApplyEventResult(result, event.focused);
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

    if (target_id == UiElementId::ToolbarDragHandle) {
        if (event.type == UiEventType::PointerDown && event.button == UiPointerButton::Left) {
            app_ui_->BeginToolbarDrag(event.point);
            return UiEventResult{
                .handled = true,
                .capture = UiCaptureRequest::Capture,
                .focus_target = UiElementId::ToolbarDragHandle,
            };
        }

        if (event.type == UiEventType::PointerMove && app_ui_->IsToolbarDragging()) {
            app_ui_->DragToolbar(event.point);
            return UiEventResult{
                .handled = true,
                .needs_render = true,
            };
        }

        if (event.type == UiEventType::PointerUp && app_ui_->IsToolbarDragging()) {
            app_ui_->EndToolbarDrag();
            return UiEventResult{
                .handled = true,
                .needs_render = true,
                .capture = UiCaptureRequest::Release,
            };
        }
    }

    UiEventResult result = {};
    if (UiElement* target = app_ui_->Root()->FindById(target_id)) {
        UiPointerEvent target_event = event;
        target_event.target = hit_id;
        target_event.captured = captured_id_;
        result = target->OnPointerEvent(target_event);
    }

    if (!result.handled && target_id != UiElementId::None) {
        result.handled = true;
    }
    result.needs_render = result.needs_render || was_hovered != hovered_id_;

    if (event.type == UiEventType::PointerDown && result.capture == UiCaptureRequest::Capture) {
        result.focus_target = target_id;
    }

    return result;
}

UiEventResult UiController::DispatchKeyEvent(const UiKeyEvent& event)
{
    if (focused_id_ == UiElementId::None) {
        return {};
    }

    if (UiElement* focused = app_ui_->Root()->FindById(focused_id_)) {
        UiKeyEvent focused_event = event;
        focused_event.focused = focused_id_;
        return focused->OnKeyEvent(focused_event);
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

void UiController::Draw(
    ID2D1DeviceContext* d2d_context,
    D2D1_SIZE_F viewport_size,
    IDWriteFactory* dwrite_factory,
    IDWriteTextFormat* body_text_format,
    IDWriteTextFormat* icon_text_format)
{
    app_ui_->Draw(
        d2d_context,
        viewport_size,
        dwrite_factory,
        body_text_format,
        icon_text_format,
        ImageViewerUiState{
            .hovered = hovered_id_,
            .pressed = pressed_id_,
        });
}

UiElementId UiController::HitTest(D2D1_POINT_2F point) const
{
    const UiElement* hit_element = app_ui_->Root()->HitTest(point);
    return hit_element != nullptr ? hit_element->Id() : UiElementId::None;
}

AppAction UiController::ActionFor(UiElementId id) const
{
    const UiElementMetadata* metadata = MetadataForElement(id);
    if (metadata == nullptr || !IsActionEnabled(metadata->action)) {
        return AppAction::None;
    }

    return metadata->action;
}

bool UiController::IsActionEnabled(AppAction action) const
{
    if (action == AppAction::None) {
        return false;
    }

    return std::find(disabled_actions_.begin(), disabled_actions_.end(), action) == disabled_actions_.end();
}

const UiElementMetadata* UiController::MetadataForElement(UiElementId id) const
{
    const UiElement* root = app_ui_->Root();
    const UiElement* element = root->FindById(id);
    return element != nullptr && element != root ? &element->Metadata() : nullptr;
}

size_t UiController::ElementCount() const
{
    return app_ui_->Root()->ChildCount();
}

const UiElementMetadata* UiController::ElementMetadataAt(size_t index) const
{
    const UiElement* element = app_ui_->Root()->ChildAt(index);
    return element != nullptr ? &element->Metadata() : nullptr;
}

const UiElementMetadata* UiController::ElementMetadata(UiElementId id) const
{
    return MetadataForElement(id);
}

D2D1_RECT_F UiController::ElementRect(UiElementId id) const
{
    const UiElement* root = app_ui_->Root();
    const UiElement* element = root->FindById(id);
    return element != nullptr && element != root ? element->Rect() : D2D1::RectF();
}

bool UiController::IsElementEnabled(UiElementId id) const
{
    const UiElement* root = app_ui_->Root();
    const UiElement* element = root->FindById(id);
    return element != nullptr && element != root && element->IsEnabled();
}

bool UiController::IsPointInCaptionDragArea(D2D1_POINT_2F point) const
{
    return app_ui_->IsPointInCaptionDragArea(point);
}

void UiController::SetTitleText(const wchar_t* title)
{
    app_ui_->SetTitleText(title);
}

void UiController::SetActionEnabled(AppAction action, bool enabled)
{
    if (action == AppAction::None) {
        return;
    }

    const auto existing = std::find(disabled_actions_.begin(), disabled_actions_.end(), action);
    if (enabled && existing != disabled_actions_.end()) {
        disabled_actions_.erase(existing);
    } else if (!enabled && existing == disabled_actions_.end()) {
        disabled_actions_.push_back(action);
    }

    SetActionEnabledRecursive(app_ui_->Root(), action, enabled);
}

void UiController::SetWindowState(bool top_most, bool maximized)
{
    app_ui_->SetWindowState(top_most, maximized);
}

void UiController::SetActionEnabledRecursive(UiElement* element, AppAction action, bool enabled)
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
