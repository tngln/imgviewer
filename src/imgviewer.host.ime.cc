#include "imgviewer.hpp"

#include "imgviewer.edit_geometry.hpp"
#include "math.hpp"
#include "ui.host_ime.hpp"
#include "ui.text.hpp"
#include "win32.util.hpp"

#include <windows.h>

#include <imm.h>

#include <algorithm>
#include <string>

#include <wil/com.h>

// Main-window IME caret/positioning. Relocated out of the message-pump file
// (imgviewer.host.cc) so the viewport/caret geometry lives in one cohesive TU
// (refactor.md A1).

namespace {

bool EditingTextCaretPoint(ImgViewerContext* context, D2D1_POINT_2F* point)
{
    if (context == nullptr || point == nullptr) {
        return false;
    }

    const ImgViewerEditSnapshot edit = context->edit.Snapshot();
    if (!edit.active || !edit.editing_text || edit.editing_text_index >= edit.texts.size()) {
        return false;
    }

    constexpr float kPaddingX = 6.0f;
    constexpr float kPaddingY = 4.0f;
    const ImgViewerEditText& text = edit.texts[edit.editing_text_index];
    const float font_size = (std::max)(6.0f, text.style.font_size);
    wil::com_ptr<IDWriteTextFormat> format;
    if (FAILED(ui_text::CreateTextFormat(
            context->graphics_device.DWriteFactory(),
            ui_text::TypeFace{
                .family = text.style.font_family,
                .size = font_size,
                .weight = DWRITE_FONT_WEIGHT_NORMAL,
            },
            format.put()))) {
        return false;
    }

    const TextEditState& edit_state = edit.editing_text_state;
    const std::wstring display_text = edit_state.DisplayText().empty() ? L" " : edit_state.DisplayText();
    const float width = (std::max)(48.0f, static_cast<float>(display_text.size()) * font_size * 0.55f + kPaddingX * 2.0f);
    wil::com_ptr<IDWriteTextLayout> layout;
    if (FAILED(context->graphics_device.DWriteFactory()->CreateTextLayout(
            display_text.c_str(),
            static_cast<UINT32>(display_text.size()),
            format.get(),
            width,
            4096.0f,
            layout.put()))) {
        return false;
    }

    const D2D1_POINT_2F origin = D2D1::Point2F(text.origin.x + kPaddingX, text.origin.y + kPaddingY);
    D2D1_POINT_2F caret_top = origin;
    D2D1_POINT_2F caret_bottom = origin;
    if (!edit_state.CaretMetrics(layout.get(), origin, &caret_top, &caret_bottom)) {
        return false;
    }
    *point = caret_bottom;
    return true;
}

D2D1_POINT_2F DocumentPointToViewportPoint(const ImgViewerSnapshot& image, const ImgViewerEditSnapshot& edit, D2D1_POINT_2F point, D2D1_SIZE_U viewport_size)
{
    const D2D1_SIZE_U preview_size = imgviewer_edit_geometry::EditPreviewSize(image.pixel_size, edit.rotation_quadrants);
    const float image_scale = math::FitScale(preview_size, viewport_size) * image.zoom_multiplier;
    const D2D1_POINT_2F viewport_center = D2D1::Point2F(
        static_cast<float>(viewport_size.width) * 0.5f,
        static_cast<float>(viewport_size.height) * 0.5f);
    const D2D1_POINT_2F preview_view_center = imgviewer_edit_geometry::SourcePointToEditPreviewPoint(
        image.view_center,
        image.pixel_size,
        edit.rotation_quadrants);
    const D2D1_MATRIX_3X2_F transform =
        imgviewer_edit_geometry::SourceToEditPreviewTransform(image.pixel_size, edit.rotation_quadrants) *
        D2D1::Matrix3x2F::Translation(-preview_view_center.x, -preview_view_center.y) *
        D2D1::Matrix3x2F::Scale(image_scale, image_scale) *
        D2D1::Matrix3x2F::Scale(
            image.flipped_horizontal ? -1.0f : 1.0f,
            image.flipped_vertical ? -1.0f : 1.0f,
            D2D1::Point2F(0.0f, 0.0f)) *
        D2D1::Matrix3x2F::Rotation(image.rotation_degrees, D2D1::Point2F(0.0f, 0.0f)) *
        D2D1::Matrix3x2F::Translation(viewport_center.x, viewport_center.y);
    return D2D1::Point2F(
        point.x * transform._11 + point.y * transform._21 + transform._31,
        point.x * transform._12 + point.y * transform._22 + transform._32);
}

} // namespace

void PositionMainWindowIme(HWND hwnd, ImgViewerContext* context)
{
    if (context == nullptr || !context->edit.IsEditingText()) {
        return;
    }

    D2D1_POINT_2F caret_document_point = {};
    if (!EditingTextCaretPoint(context, &caret_document_point)) {
        return;
    }

    const ImgViewerSnapshot image = context->viewer.Snapshot();
    const ImgViewerEditSnapshot edit = context->edit.Snapshot();
    const D2D1_SIZE_U viewport_size = context->renderer.ViewportPixelSize();
    if (image.bitmap == nullptr || viewport_size.width == 0 || viewport_size.height == 0) {
        return;
    }

    const D2D1_POINT_2F caret_viewport_point = DocumentPointToViewportPoint(image, edit, caret_document_point, viewport_size);
    SetImeCompositionWindowClientPoint(hwnd, caret_viewport_point);
}

void SyncImgViewerMainWindowIme(HWND hwnd, ImgViewerContext* context)
{
    if (hwnd == nullptr || context == nullptr) {
        return;
    }

    const bool should_enable = context->edit.IsEditingText() && context->main_window_ime_context != nullptr;
    if (context->main_window_ime_enabled == should_enable) {
        if (should_enable) {
            PositionMainWindowIme(hwnd, context);
        }
        return;
    }

    if (!should_enable) {
        if (HIMC ime = ImmGetContext(hwnd)) {
            ImmNotifyIME(ime, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
            ImmReleaseContext(hwnd, ime);
        }
    }

    util::AssociateImeContext(hwnd, should_enable ? context->main_window_ime_context : nullptr);
    context->main_window_ime_enabled = should_enable;
    if (should_enable) {
        PositionMainWindowIme(hwnd, context);
    }
}
