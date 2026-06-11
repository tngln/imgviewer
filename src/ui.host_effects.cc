#include "ui.host_effects.hpp"

UiPostedActionMessage DecodeUiPostedActionMessage(WPARAM wparam, LPARAM lparam)
{
    return UiPostedActionMessage{
        .action = UiAction(static_cast<int>(wparam)),
        .effect_target = static_cast<UiElementId>(static_cast<int>(lparam)),
    };
}

void ApplyUiCaptureRequest(HWND hwnd, UiCaptureRequest capture)
{
    if (capture == UiCaptureRequest::Capture) {
        SetCapture(hwnd);
    } else if (capture == UiCaptureRequest::Release) {
        ReleaseCapture();
    }
}

bool ApplyUiEffect(UiController* ui, UiElementId effect_target)
{
    if (ui == nullptr || effect_target == UiElementId::None || ui->Root() == nullptr) {
        return false;
    }

    ui->Root()->ApplyElementEffect(effect_target);
    return true;
}

bool ApplyUiEffectAndInvalidate(HWND hwnd, UiController* ui, UiElementId effect_target)
{
    if (!ApplyUiEffect(ui, effect_target)) {
        return false;
    }

    RequestWindowRender(hwnd);
    return true;
}

void RequestWindowRender(HWND hwnd, bool needs_render)
{
    if (hwnd != nullptr && needs_render) {
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}
