#include "imgviewer.host.internal.hpp"

#include "imgviewer.keybindings.hpp"
#include "imgviewer.ui.action.hpp"
#include "ui.host_ime.hpp"
#include "win32.util.hpp"

#include <windows.h>

#include <imm.h>

win32::WindowMessageResult HandleImgViewerKeyboardMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        UiKeyEvent key{
            .type = UiEventType::KeyDown,
            .virtual_key = static_cast<UINT>(wparam),
            .modifiers = UiModifiers::Current(),
            .repeat = (lparam & 0x40000000) != 0,
            .system = message == WM_SYSKEYDOWN,
            .popup_host = context != nullptr ? &context->popup : nullptr,
        };
        if (DispatchToPopup(hwnd, context, UiInputEvent::Key(key, hwnd))) {
            return win32::WindowMessageResult::Handled();
        }

        SyncKeyboardOwner(context);
        if (context != nullptr && context->interaction.KeyboardOwner() == ImgViewerKeyboardOwner::EditText) {
            if (!key.modifiers.alt && key.modifiers.ctrl) {
                UiAction text_action = kUiActionNone;
                if (wparam == 'A') {
                    text_action = kUiActionTextSelectAll;
                } else if (wparam == 'C') {
                    text_action = kUiActionTextCopy;
                } else if (wparam == 'X') {
                    text_action = kUiActionTextCut;
                } else if (wparam == 'V') {
                    text_action = kUiActionTextPaste;
                }
                if (text_action != kUiActionNone) {
                    if (context->edit.ExecuteTextEditAction(text_action, hwnd)) {
                        ApplyRenderAndIme(hwnd, context);
                    }
                    return win32::WindowMessageResult::Handled();
                }
            }
            if (!key.modifiers.ctrl && !key.modifiers.alt) {
                if (context->edit.OnTextKeyDown(static_cast<UINT>(wparam), key.modifiers.shift)) {
                    ApplyRenderAndIme(hwnd, context);
                }
                return win32::WindowMessageResult::Handled();
            }
        }

        if (context != nullptr &&
            wparam == VK_ESCAPE &&
            !key.modifiers.ctrl &&
            !key.modifiers.shift &&
            !key.modifiers.alt &&
            context->edit.Active() &&
            context->edit.Tool() == ImgViewerEditTool::Crop) {
            ExecuteImgViewerAction(hwnd, context, UiAction(ImgViewerAction::EditCancelCrop));
            return win32::WindowMessageResult::Handled();
        }
        if (context != nullptr &&
            wparam == VK_ESCAPE &&
            !key.modifiers.ctrl &&
            !key.modifiers.shift &&
            !key.modifiers.alt &&
            context->edit.Active() &&
            context->edit.Tool() == ImgViewerEditTool::Select &&
            context->edit.HasSelection()) {
            context->edit.CancelSelection();
            ApplyRenderAndIme(hwnd, context);
            return win32::WindowMessageResult::Handled();
        }

        const UiEventResult ui_result = context != nullptr && context->ui != nullptr
            ? context->ui->OnInputEvent(UiInputEvent::Key(key, hwnd))
            : UiEventResult{};
        if (ui_result.handled) {
            ApplyMerged(hwnd, context, ui_result);
            return win32::WindowMessageResult::Handled();
        }

        if (message == WM_KEYDOWN && wparam == 'V' && key.modifiers.ctrl && !key.modifiers.shift && !key.modifiers.alt) {
            HandleImgViewerPasteClipboard(hwnd, context);
            return win32::WindowMessageResult::Handled();
        }
        if (message == WM_KEYDOWN && wparam == 'S' && key.modifiers.ctrl && !key.modifiers.shift && !key.modifiers.alt) {
            ExecuteImgViewerAction(hwnd, context, UiAction(ImgViewerAction::SaveImageAs));
            return win32::WindowMessageResult::Handled();
        }

        const ImgViewerAction action = ActionForKeyboardMessage(context, wparam);
        if (context != nullptr) {
            context->pressed_key_actions[KeyActionIndex(wparam)] = action;
        }
        if (context != nullptr &&
            context->interaction.CanvasOwner() == ImgViewerCanvasOwner::Viewer &&
            context->viewer.OnActionDown(action)) {
            return win32::WindowMessageResult::Handled();
        }
        if (action == ImgViewerAction::OpenImage) {
            HandleImgViewerOpenImageCommand(hwnd, context);
            return win32::WindowMessageResult::Handled();
        }
        if (action != ImgViewerAction::None) {
            ExecuteImgViewerAction(hwnd, context, UiAction(action));
            ApplyImeSync(hwnd, context);
            return win32::WindowMessageResult::Handled();
        }
        return win32::WindowMessageResult::Unhandled();
    }

    case WM_CHAR: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        SyncKeyboardOwner(context);
        if (context != nullptr &&
            context->interaction.KeyboardOwner() == ImgViewerKeyboardOwner::EditText &&
            context->edit.OnTextInput(static_cast<wchar_t>(wparam))) {
            ApplyRenderAndIme(hwnd, context);
            return win32::WindowMessageResult::Handled();
        }
        return win32::WindowMessageResult::Unhandled();
    }

    case WM_IME_STARTCOMPOSITION: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        SyncKeyboardOwner(context);
        if (context != nullptr && context->interaction.KeyboardOwner() == ImgViewerKeyboardOwner::EditText) {
            ApplyImeSync(hwnd, context);
            return win32::WindowMessageResult::Handled();
        }
        return win32::WindowMessageResult::Unhandled();
    }

    case WM_IME_COMPOSITION: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        SyncKeyboardOwner(context);
        if (context != nullptr && context->interaction.KeyboardOwner() == ImgViewerKeyboardOwner::EditText) {
            bool changed = false;
            if ((lparam & GCS_RESULTSTR) != 0) {
                changed = context->edit.CommitTextImeResult(ReadImeCompositionString(hwnd, lparam, GCS_RESULTSTR)) || changed;
            }
            if ((lparam & GCS_COMPSTR) != 0) {
                changed = context->edit.UpdateTextImeComposition(ReadImeCompositionString(hwnd, lparam, GCS_COMPSTR)) || changed;
            } else if ((lparam & GCS_RESULTSTR) != 0) {
                changed = context->edit.EndTextImeComposition() || changed;
            }
            if (changed) {
                ApplyRenderAndIme(hwnd, context);
            }
            return win32::WindowMessageResult::Handled();
        }
        return win32::WindowMessageResult::Unhandled();
    }

    case WM_IME_ENDCOMPOSITION: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        SyncKeyboardOwner(context);
        if (context != nullptr && context->interaction.KeyboardOwner() == ImgViewerKeyboardOwner::EditText) {
            if (context->edit.EndTextImeComposition()) {
                ApplyRenderAndIme(hwnd, context);
            }
            return win32::WindowMessageResult::Handled();
        }
        return win32::WindowMessageResult::Unhandled();
    }

    case WM_KEYUP:
    case WM_SYSKEYUP: {
        ImgViewerContext* context = GetImgViewerContext(hwnd);
        UiKeyEvent key{
            .type = UiEventType::KeyUp,
            .virtual_key = static_cast<UINT>(wparam),
            .modifiers = UiModifiers::Current(),
            .system = message == WM_SYSKEYUP,
            .popup_host = context != nullptr ? &context->popup : nullptr,
        };
        if (DispatchToPopup(hwnd, context, UiInputEvent::Key(key, hwnd))) {
            return win32::WindowMessageResult::Handled();
        }
        const UiEventResult ui_result = context != nullptr && context->ui != nullptr
            ? context->ui->OnInputEvent(UiInputEvent::Key(key, hwnd))
            : UiEventResult{};
        if (ui_result.handled) {
            ApplyMerged(hwnd, context, ui_result);
            return win32::WindowMessageResult::Handled();
        }
        const ImgViewerAction action =
            context != nullptr ? context->pressed_key_actions[KeyActionIndex(wparam)] : ImgViewerAction::None;
        if (context != nullptr) {
            context->pressed_key_actions[KeyActionIndex(wparam)] = ImgViewerAction::None;
        }
        const ImgViewerEventResult viewer_result =
            context != nullptr && context->interaction.CanvasOwner() == ImgViewerCanvasOwner::Viewer
                ? context->viewer.OnActionUp(action)
                : ImgViewerEventResult{};
        if (viewer_result.handled) {
            ApplyMerged(hwnd, context, viewer_result);
            return win32::WindowMessageResult::Handled();
        }
        return win32::WindowMessageResult::Unhandled();
    }

    default:
        return win32::WindowMessageResult::Unhandled();
    }
}
