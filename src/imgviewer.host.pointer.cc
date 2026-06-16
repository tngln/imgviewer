#include "imgviewer.host.internal.hpp"

#include "imgviewer.host.pointer_router.hpp"
#include "math.hpp"
#include "ui.host_input.hpp"
#include "win32.util.hpp"

#include <windows.h>
#include <windowsx.h>

win32::WindowMessageResult HandleImgViewerPointerMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_MOUSEMOVE: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        if (context != nullptr && context->config.borderless_window) {
            context->renderer.SetUiOverlayVisible(true);
        }
        const D2D1_POINT_2F point = GetPointerPoint(hwnd, lparam);
        util::TrackMouseLeave(hwnd);
        if (context != nullptr) {
            const D2D1_POINT_2F ui_point = ui_host_input::PhysicalClientPointToUi(hwnd, lparam);
            UiPointerEvent pointer{
                .type = UiEventType::PointerMove,
                .point = ui_point,
                .modifiers = UiModifiers::Current(),
                .popup_host = &context->popup,
            };
            if (DispatchToPopup(hwnd, context, UiInputEvent::Pointer(pointer, hwnd))) {
                return win32::WindowMessageResult::Handled();
            }
            if (CanUiReceivePointer(context->interaction)) {
                UiEventResult ui_result = context->ui != nullptr
                    ? context->ui->OnInputEvent(UiInputEvent::Pointer(pointer, hwnd))
                    : UiEventResult{};
                ApplyMerged(hwnd, context, ui_result);
                if (ui_result.handled) {
                    return win32::WindowMessageResult::Handled();
                }
            }

            if (CancelPendingEdgeClickIfDragged(hwnd, context, point)) {
                return win32::WindowMessageResult::Handled();
            }

            ImgViewerEventResult canvas_result = {};
            switch (ActivePointerTarget(context->interaction, context->edit.Active())) {
            case ImgViewerPointerTarget::ColorPicker:
                if (UpdateImgViewerColorPickerSample(context, point)) {
                    ApplyRender(hwnd, context);
                    return win32::WindowMessageResult::Handled();
                }
                break;
            case ImgViewerPointerTarget::EditTool:
                canvas_result = context->edit.OnPointerMove(point, context->viewer.Snapshot(), context->renderer.ViewportPixelSize());
                break;
            case ImgViewerPointerTarget::Viewer:
                canvas_result = context->viewer.OnPointerMove(point.x, point.y, context->renderer.ViewportPixelSize());
                break;
            default:
                break;
            }
            ApplyMerged(hwnd, context, canvas_result);
        }
        return win32::WindowMessageResult::Handled();
    }

    case WM_LBUTTONDOWN: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        const D2D1_POINT_2F point = GetPointerPoint(hwnd, lparam);
        const D2D1_POINT_2F ui_point = ui_host_input::PhysicalClientPointToUi(hwnd, lparam);
        UiPointerEvent pointer{
            .type = UiEventType::PointerDown,
            .point = ui_point,
            .button = UiPointerButton::Left,
            .modifiers = UiModifiers::Current(),
            .popup_host = context != nullptr ? &context->popup : nullptr,
        };
        if (DispatchToPopup(hwnd, context, UiInputEvent::Pointer(pointer, hwnd))) {
            return win32::WindowMessageResult::Handled();
        }
        UiEventResult ui_result = context != nullptr && context->ui != nullptr
            ? context->ui->OnInputEvent(UiInputEvent::Pointer(pointer, hwnd))
            : UiEventResult{};
        ImgViewerEventResult viewer_result = {};
        if (context != nullptr && !ui_result.handled) {
            const ImgViewerAction edge_click_action = EdgeClickActionAtPoint(context, point);
            const ImgViewerPointerTarget canvas_target = CanvasPointerTarget(context->interaction, context->edit.Active());
            if (edge_click_action != ImgViewerAction::None) {
                context->pending_edge_click_action = edge_click_action;
                context->pending_edge_click_point = point;
                ImgViewerHostEffects capture_effects;
                capture_effects.begin_pointer_capture = ImgViewerPointerCaptureOwner::EdgeClickNavigation;
                ApplyHostEffects(hwnd, context, capture_effects);
                ui_result.handled = true;
            } else if (canvas_target == ImgViewerPointerTarget::ColorPicker &&
                UpdateImgViewerColorPickerSample(context, point)) {
                ImgViewerHostEffects capture_effects;
                capture_effects.begin_pointer_capture = ImgViewerPointerCaptureOwner::ColorPicker;
                ApplyHostEffects(hwnd, context, capture_effects);
                ui_result.handled = true;
            } else if (canvas_target == ImgViewerPointerTarget::EditTool) {
                viewer_result = context->edit.OnPointerDown(point, context->viewer.Snapshot(), context->renderer.ViewportPixelSize());
            } else {
                viewer_result = context->viewer.OnPointerDown(point.x, point.y, context->renderer.ViewportPixelSize());
            }
        }
        if (viewer_result.captured) {
            ImgViewerHostEffects capture_effects;
            if (context != nullptr && context->edit.HasTransientCapture()) {
                capture_effects.begin_pointer_capture = EditPointerCaptureOwner(context->edit);
            } else if (context != nullptr && context->viewer.HasTransientCapture()) {
                capture_effects.begin_pointer_capture = util::IsKeyDown('R')
                    ? ImgViewerPointerCaptureOwner::ViewerRotate
                    : ImgViewerPointerCaptureOwner::ViewerPan;
            }
            ApplyHostEffects(hwnd, context, capture_effects);
        }
        ImgViewerHostEffects effects;
        effects.Merge(ui_result);
        effects.Merge(viewer_result);
        ApplyHostEffects(hwnd, context, effects);
        return (ui_result.handled || viewer_result.handled)
            ? win32::WindowMessageResult::Handled()
            : win32::WindowMessageResult::Unhandled();
    }

    case WM_LBUTTONDBLCLK: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        const D2D1_POINT_2F point = GetPointerPoint(hwnd, lparam);
        ImgViewerEventResult edit_result = {};
        if (context != nullptr &&
            CanvasPointerTarget(context->interaction, context->edit.Active()) == ImgViewerPointerTarget::EditTool) {
            edit_result = context->edit.OnPointerDoubleClick(
                point,
                context->viewer.Snapshot(),
                context->renderer.ViewportPixelSize());
        }
        ApplyMerged(hwnd, context, edit_result);
        return edit_result.handled
            ? win32::WindowMessageResult::Handled()
            : win32::WindowMessageResult::Unhandled();
    }

    case WM_LBUTTONUP: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        const D2D1_POINT_2F point = GetPointerPoint(hwnd, lparam);
        ImgViewerEventResult viewer_result = {};
        if (context != nullptr) {
            const ImgViewerPointerTarget captured_target = CapturedPointerTarget(context->interaction.PointerCapture());
            if (CommitPendingEdgeClick(hwnd, context, point)) {
                viewer_result = ImgViewerEventResult{.handled = true};
            } else if (captured_target == ImgViewerPointerTarget::ColorPicker) {
                ImgViewerHostEffects capture_effects;
                capture_effects.end_pointer_capture = ImgViewerPointerCaptureOwner::ColorPicker;
                ApplyHostEffects(hwnd, context, capture_effects);
                viewer_result = ImgViewerEventResult{.handled = true};
            } else if (captured_target == ImgViewerPointerTarget::EditTool) {
                viewer_result = context->edit.OnPointerUp(point, context->viewer.Snapshot(), context->renderer.ViewportPixelSize());
            } else if (captured_target == ImgViewerPointerTarget::Viewer) {
                viewer_result = context->viewer.OnPointerUp(point.x, point.y, context->renderer.ViewportPixelSize());
            }
        }
        ApplyMerged(hwnd, context, viewer_result);
        UiEventResult ui_result = {};
        if (context != nullptr &&
            !viewer_result.handled &&
            CanUiReceivePointer(context->interaction)) {
            const D2D1_POINT_2F ui_point = ui_host_input::PhysicalClientPointToUi(hwnd, lparam);
            UiPointerEvent pointer{
                .type = UiEventType::PointerUp,
                .point = ui_point,
                .button = UiPointerButton::Left,
                .modifiers = UiModifiers::Current(),
                .popup_host = &context->popup,
            };
            if (DispatchToPopup(hwnd, context, UiInputEvent::Pointer(pointer, hwnd))) {
                return win32::WindowMessageResult::Handled();
            }
            ui_result = context->ui != nullptr
                ? context->ui->OnInputEvent(UiInputEvent::Pointer(pointer, hwnd))
                : UiEventResult{};
            ApplyMerged(hwnd, context, ui_result);
        }
        return (ui_result.handled || viewer_result.handled)
            ? win32::WindowMessageResult::Handled()
            : win32::WindowMessageResult::Unhandled();
    }

    case WM_MOUSELEAVE: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        UiPointerEvent pointer{.type = UiEventType::PointerLeave, .modifiers = UiModifiers::Current()};
        ApplyMerged(
            hwnd,
            context,
            context != nullptr && context->ui != nullptr ? context->ui->OnInputEvent(UiInputEvent{
                .type = pointer.type,
                .pointer = pointer,
                .hwnd = hwnd,
                .popup_host = &context->popup,
            }) : UiEventResult{});
        if (context != nullptr &&
            context->config.borderless_window &&
            !context->interaction.HasPointerCapture() &&
            !IsCursorInsideWindow(hwnd)) {
            context->renderer.SetUiOverlayVisible(false);
        }
        return win32::WindowMessageResult::Handled();
    }

    case WM_MOUSEWHEEL: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        const D2D1_POINT_2F point = GetScreenPointerPoint(hwnd, lparam);
        if (context != nullptr && util::IsKeyDown('O')) {
            constexpr int kOpacityWheelStep = 5;
            const int wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam);
            const int steps = wheel_delta / WHEEL_DELTA;
            if (steps != 0) {
                SetImgViewerWindowOpacity(
                    hwnd,
                    context,
                    context->current_window_opacity_percent + steps * kOpacityWheelStep);
            }
            return win32::WindowMessageResult::Handled();
        }
        if (context != nullptr) {
            const float dpi_scale = math::CoordinateSpace::FromWindow(hwnd).scale();
            const D2D1_POINT_2F ui_point = D2D1::Point2F(point.x / dpi_scale, point.y / dpi_scale);
            UiPointerEvent pointer{
                .type = UiEventType::PointerWheel,
                .point = ui_point,
                .wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam),
                .modifiers = UiModifiers::Current(),
                .popup_host = &context->popup,
            };
            if (DispatchToPopup(hwnd, context, UiInputEvent::Pointer(pointer, hwnd))) {
                return win32::WindowMessageResult::Handled();
            }
            const UiEventResult ui_result = context->ui != nullptr
                ? context->ui->OnInputEvent(UiInputEvent::Pointer(pointer, hwnd))
                : UiEventResult{};
            if (ui_result.handled) {
                ApplyMerged(hwnd, context, ui_result);
                return win32::WindowMessageResult::Handled();
            }
        }
        if (context != nullptr &&
            CanvasPointerTarget(context->interaction, context->edit.Active()) == ImgViewerPointerTarget::Viewer &&
            context->viewer.OnMouseWheel(
                point.x,
                point.y,
                GET_WHEEL_DELTA_WPARAM(wparam),
                context->renderer.ViewportPixelSize())) {
            ApplyRender(hwnd, context);
            return win32::WindowMessageResult::Handled();
        }
        return win32::WindowMessageResult::Unhandled();
    }

    default:
        return win32::WindowMessageResult::Unhandled();
    }
}
