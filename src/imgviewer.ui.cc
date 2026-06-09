#include "imgviewer.ui.hpp"

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include <d2d1helper.h>

#include "imgviewer.ui.action.hpp"
#include "ui.popup.hpp"
#include "ui.theme.hpp"

namespace {

constexpr D2D1_POINT_2F kMainMenuOrigin{3.0f, ui_theme::metrics::kTitleBarHeight + 2.0f};

bool SameColor(D2D1_COLOR_F left, D2D1_COLOR_F right)
{
    constexpr float kTolerance = 0.001f;
    return std::abs(left.r - right.r) < kTolerance &&
        std::abs(left.g - right.g) < kTolerance &&
        std::abs(left.b - right.b) < kTolerance &&
        std::abs(left.a - right.a) < kTolerance;
}

} // namespace

ImgViewerUi::ImgViewerUi() :
    root_(std::make_unique<UiElement>(
        UiRootMetadata(UiElementRole::Pane, kUiActionNone, L"ImgViewer", L"", L"root"))),
    titlebar_(*root_),
    toolbar_(*root_),
    color_picker_toolstrip_(*root_),
    edit_toolbar_(*root_),
    pen_toolstrip_(*root_),
    shape_toolstrip_(*root_),
    text_toolstrip_(*root_),
    selection_toolstrip_(*root_),
    animation_toolbar_(*root_),
    info_panel_(*root_)
{
}

UiElement* ImgViewerUi::Root()
{
    return root_.get();
}

const UiElement* ImgViewerUi::Root() const
{
    return root_.get();
}

const wchar_t* ImgViewerUi::AccessibilityRootName() const
{
    return L"ImgViewer";
}

D2D1_SIZE_F ImgViewerUi::Measure(const UiDrawContext& context, D2D1_SIZE_F available_size)
{
    titlebar_.Measure(context, available_size);
    toolbar_.Measure(context, available_size);
    color_picker_toolstrip_.Measure(context, available_size);
    edit_toolbar_.Measure(context, available_size);
    pen_toolstrip_.Measure(context, available_size);
    shape_toolstrip_.Measure(context, available_size);
    text_toolstrip_.Measure(context, available_size);
    selection_toolstrip_.Measure(context, available_size);
    animation_toolbar_.Measure(context, available_size);
    info_panel_.Measure(context, available_size);
    return available_size;
}

void ImgViewerUi::Arrange(D2D1_RECT_F final_rect)
{
    root_->Arrange(final_rect);
    titlebar_.Arrange(D2D1::RectF(final_rect.left, final_rect.top, final_rect.right, final_rect.top + ui_theme::metrics::kTitleBarHeight));
    toolbar_.Arrange(final_rect);
    color_picker_toolstrip_.Arrange(final_rect, toolbar_.Rect());
    edit_toolbar_.Arrange(final_rect, toolbar_.Rect());
    selection_toolstrip_.Arrange(final_rect, edit_toolbar_.Rect());
    pen_toolstrip_.Arrange(final_rect, edit_toolbar_.Rect());
    shape_toolstrip_.Arrange(final_rect, edit_toolbar_.Rect());
    text_toolstrip_.Arrange(final_rect, edit_toolbar_.Rect());
    animation_toolbar_.Arrange(
        final_rect,
        color_picker_toolstrip_state_.visible
            ? color_picker_toolstrip_.Rect()
            : (text_toolstrip_state_.visible
            ? text_toolstrip_.Rect()
            : (pen_toolstrip_state_.visible
                ? pen_toolstrip_.Rect()
                : (shape_toolstrip_state_.visible
                    ? shape_toolstrip_.Rect()
                    : (selection_toolstrip_state_.visible ? selection_toolstrip_.Rect() : (edit_toolbar_state_.visible ? edit_toolbar_.Rect() : toolbar_.Rect()))))));
    info_panel_.Arrange(final_rect);
}

void ImgViewerUi::Render(
    const UiDrawContext& draw_context,
    UiRootState state)
{
    titlebar_.Render(draw_context, state, top_most_, maximized_, edit_toolbar_state_.visible);
    toolbar_.Render(draw_context, state, color_picker_active_);
    color_picker_toolstrip_.Render(draw_context, state);
    edit_toolbar_.Render(draw_context, state);
    text_toolstrip_.Render(draw_context, state);
    pen_toolstrip_.Render(draw_context, state);
    shape_toolstrip_.Render(draw_context, state);
    selection_toolstrip_.Render(draw_context, state);
    animation_toolbar_.Render(draw_context, state);
    info_panel_.Render(draw_context);
    toast_.Render(draw_context);
}

UiEventResult ImgViewerUi::OnPointerEvent(const UiPointerEvent& event)
{
    UiEventResult info_panel_result = info_panel_.OnPointerEvent(event);
    if (info_panel_result.handled) {
        return info_panel_result;
    }
    UiEventResult color_picker_toolstrip_result = color_picker_toolstrip_.OnPointerEvent(event);
    if (color_picker_toolstrip_result.handled) {
        return color_picker_toolstrip_result;
    }
    UiEventResult selection_toolstrip_result = selection_toolstrip_.OnPointerEvent(event);
    if (selection_toolstrip_result.handled) {
        return selection_toolstrip_result;
    }
    UiEventResult text_toolstrip_result = text_toolstrip_.OnPointerEvent(event);
    if (text_toolstrip_result.handled) {
        return text_toolstrip_result;
    }
    UiEventResult pen_toolstrip_result = pen_toolstrip_.OnPointerEvent(event);
    if (pen_toolstrip_result.handled) {
        return pen_toolstrip_result;
    }
    UiEventResult shape_toolstrip_result = shape_toolstrip_.OnPointerEvent(event);
    if (shape_toolstrip_result.handled) {
        return shape_toolstrip_result;
    }
    UiEventResult edit_toolbar_result = edit_toolbar_.OnPointerEvent(event);
    if (edit_toolbar_result.handled) {
        return edit_toolbar_result;
    }
    UiEventResult animation_toolbar_result = animation_toolbar_.OnPointerEvent(event);
    if (animation_toolbar_result.handled) {
        return animation_toolbar_result;
    }
    return toolbar_.OnPointerEvent(event);
}

UiEventResult ImgViewerUi::OnKeyEvent(const UiKeyEvent& event)
{
    UNREFERENCED_PARAMETER(event);
    return {};
}

bool ImgViewerUi::HandleUiAction(UiAction action, PopupHost* popup_host)
{
    if (action != UiActionFromImgViewerAction(ImgViewerAction::OpenMenu)) {
        return false;
    }
    if (popup_host == nullptr) {
        return false;
    }

    const bool edit_enabled = edit_toolbar_state_.visible;
    return SUCCEEDED(popup_host->OpenMenu(
        kMainMenuOrigin,
        std::vector<MenuItem>{
            {L"Open Image", UiActionFromImgViewerAction(ImgViewerAction::OpenImage)},
            {L"Desktop Screenshot", UiActionFromImgViewerAction(ImgViewerAction::CaptureDesktop)},
            {L"Region Screenshot", UiActionFromImgViewerAction(ImgViewerAction::CaptureRegion)},
            {L"Save As", UiActionFromImgViewerAction(ImgViewerAction::SaveImageAs), false, false, save_image_as_enabled_},
            {L"Show in File Explorer", UiActionFromImgViewerAction(ImgViewerAction::ShowInFileExplorer), false, false, show_in_file_explorer_enabled_},
            {L"", kUiActionNone, true},
            {L"Settings", UiActionFromImgViewerAction(ImgViewerAction::OpenSettings)},
            {L"About", UiActionFromImgViewerAction(ImgViewerAction::OpenAbout)},
            {L"", kUiActionNone, true},
            {L"Info Panel", UiActionFromImgViewerAction(ImgViewerAction::ToggleInfoPanel), false, info_panel_.IsVisible()},
            {L"Loop Animation", UiActionFromImgViewerAction(ImgViewerAction::ToggleAnimationLoop), false, animation_state_.loop, animation_state_.available},
            {animation_state_.playing ? L"Pause Animation" : L"Play Animation", UiActionFromImgViewerAction(ImgViewerAction::ToggleAnimationPlayback), false, false, animation_state_.available},
            {L"Previous Animation Frame", UiActionFromImgViewerAction(ImgViewerAction::PreviousAnimationFrame), false, false, animation_state_.available},
            {L"Next Animation Frame", UiActionFromImgViewerAction(ImgViewerAction::NextAnimationFrame), false, false, animation_state_.available},
            {L"", kUiActionNone, true},
            {L"Zoom In", UiActionFromImgViewerAction(ImgViewerAction::ZoomIn)},
            {L"Zoom Out", UiActionFromImgViewerAction(ImgViewerAction::ZoomOut)},
            {L"Fit Window", UiActionFromImgViewerAction(ImgViewerAction::FitWindow)},
            {L"Actual Size", UiActionFromImgViewerAction(ImgViewerAction::ActualSize)},
            {L"Reset View", UiActionFromImgViewerAction(ImgViewerAction::ResetView)},
            {L"Color Picker", UiActionFromImgViewerAction(ImgViewerAction::ToggleColorPicker), false, color_picker_active_},
            {L"", kUiActionNone, true},
            {L"Edit Mode", kUiActionNone, false, edit_toolbar_state_.visible, true, std::vector<MenuItem>{
                {L"Toggle Edit Mode", UiActionFromImgViewerAction(ImgViewerAction::ToggleEditMode), false, edit_toolbar_state_.visible},
                {L"", kUiActionNone, true},
                {L"Tool", kUiActionNone, false, false, edit_enabled, std::vector<MenuItem>{
                    {L"Select", UiActionFromImgViewerAction(ImgViewerAction::EditSelect), false, edit_toolbar_state_.tool == ImgViewerEditTool::Select, edit_enabled},
                    {L"Pixel Select", UiActionFromImgViewerAction(ImgViewerAction::EditPixelSelect), false, edit_toolbar_state_.tool == ImgViewerEditTool::PixelSelect, edit_enabled},
                    {L"Pen", UiActionFromImgViewerAction(ImgViewerAction::EditPen), false, edit_toolbar_state_.tool == ImgViewerEditTool::Pen, edit_enabled},
                    {L"Shape", UiActionFromImgViewerAction(ImgViewerAction::EditShape), false, edit_toolbar_state_.tool == ImgViewerEditTool::Shape, edit_enabled},
                    {L"Text", UiActionFromImgViewerAction(ImgViewerAction::EditText), false, edit_toolbar_state_.tool == ImgViewerEditTool::Text, edit_enabled},
                    {L"Crop", UiActionFromImgViewerAction(ImgViewerAction::EditCrop), false, edit_toolbar_state_.tool == ImgViewerEditTool::Crop, edit_enabled},
                }},
                {L"Pen", kUiActionNone, false, edit_toolbar_state_.tool == ImgViewerEditTool::Pen, edit_enabled, std::vector<MenuItem>{
                    {L"Pen Red", UiActionFromImgViewerAction(ImgViewerAction::EditPenColorRed), false, SameColor(pen_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Red)), edit_enabled},
                    {L"Pen Yellow", UiActionFromImgViewerAction(ImgViewerAction::EditPenColorYellow), false, SameColor(pen_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Yellow)), edit_enabled},
                    {L"Pen Green", UiActionFromImgViewerAction(ImgViewerAction::EditPenColorGreen), false, SameColor(pen_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Lime)), edit_enabled},
                    {L"Pen Cyan", UiActionFromImgViewerAction(ImgViewerAction::EditPenColorCyan), false, SameColor(pen_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Cyan)), edit_enabled},
                    {L"Pen Blue", UiActionFromImgViewerAction(ImgViewerAction::EditPenColorBlue), false, SameColor(pen_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::DodgerBlue)), edit_enabled},
                    {L"Pen Magenta", UiActionFromImgViewerAction(ImgViewerAction::EditPenColorMagenta), false, SameColor(pen_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Magenta)), edit_enabled},
                    {L"Pen White", UiActionFromImgViewerAction(ImgViewerAction::EditPenColorWhite), false, SameColor(pen_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::White)), edit_enabled},
                    {L"Pen Black", UiActionFromImgViewerAction(ImgViewerAction::EditPenColorBlack), false, SameColor(pen_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Black)), edit_enabled},
                    {L"", kUiActionNone, true},
                    {L"Pen 2 px", UiActionFromImgViewerAction(ImgViewerAction::EditPenWidth2), false, std::abs(pen_toolstrip_state_.width - 2.0f) < 0.01f, edit_enabled},
                    {L"Pen 4 px", UiActionFromImgViewerAction(ImgViewerAction::EditPenWidth4), false, std::abs(pen_toolstrip_state_.width - 4.0f) < 0.01f, edit_enabled},
                    {L"Pen 8 px", UiActionFromImgViewerAction(ImgViewerAction::EditPenWidth8), false, std::abs(pen_toolstrip_state_.width - 8.0f) < 0.01f, edit_enabled},
                    {L"Pen 12 px", UiActionFromImgViewerAction(ImgViewerAction::EditPenWidth12), false, std::abs(pen_toolstrip_state_.width - 12.0f) < 0.01f, edit_enabled},
                }},
                {L"Shape", kUiActionNone, false, edit_toolbar_state_.tool == ImgViewerEditTool::Shape, edit_enabled, std::vector<MenuItem>{
                    {L"Shape Rectangle", UiActionFromImgViewerAction(ImgViewerAction::EditShapeRectangle), false, shape_toolstrip_state_.kind == ImgViewerShapeKind::Rectangle, edit_enabled},
                    {L"Shape Ellipse", UiActionFromImgViewerAction(ImgViewerAction::EditShapeEllipse), false, shape_toolstrip_state_.kind == ImgViewerShapeKind::Ellipse, edit_enabled},
                    {L"Shape Line", UiActionFromImgViewerAction(ImgViewerAction::EditShapeLine), false, shape_toolstrip_state_.kind == ImgViewerShapeKind::Line, edit_enabled},
                    {L"Shape Arrow", UiActionFromImgViewerAction(ImgViewerAction::EditShapeArrow), false, shape_toolstrip_state_.kind == ImgViewerShapeKind::Arrow, edit_enabled},
                    {L"", kUiActionNone, true},
                    {L"Shape Red", UiActionFromImgViewerAction(ImgViewerAction::EditPenColorRed), false, SameColor(shape_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Red)), edit_enabled},
                    {L"Shape Yellow", UiActionFromImgViewerAction(ImgViewerAction::EditPenColorYellow), false, SameColor(shape_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Yellow)), edit_enabled},
                    {L"Shape Green", UiActionFromImgViewerAction(ImgViewerAction::EditPenColorGreen), false, SameColor(shape_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Lime)), edit_enabled},
                    {L"Shape Cyan", UiActionFromImgViewerAction(ImgViewerAction::EditPenColorCyan), false, SameColor(shape_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Cyan)), edit_enabled},
                    {L"Shape Blue", UiActionFromImgViewerAction(ImgViewerAction::EditPenColorBlue), false, SameColor(shape_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::DodgerBlue)), edit_enabled},
                    {L"Shape Magenta", UiActionFromImgViewerAction(ImgViewerAction::EditPenColorMagenta), false, SameColor(shape_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Magenta)), edit_enabled},
                    {L"Shape White", UiActionFromImgViewerAction(ImgViewerAction::EditPenColorWhite), false, SameColor(shape_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::White)), edit_enabled},
                    {L"Shape Black", UiActionFromImgViewerAction(ImgViewerAction::EditPenColorBlack), false, SameColor(shape_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Black)), edit_enabled},
                }},
                {L"Text", kUiActionNone, false, edit_toolbar_state_.tool == ImgViewerEditTool::Text, edit_enabled, std::vector<MenuItem>{
                    {L"Size", kUiActionNone, false, false, edit_enabled, std::vector<MenuItem>{
                        {L"Text 12 px", UiActionFromImgViewerAction(ImgViewerAction::EditTextSize12), false, std::abs(text_toolstrip_state_.style.font_size - 12.0f) < 0.01f, edit_enabled},
                        {L"Text 16 px", UiActionFromImgViewerAction(ImgViewerAction::EditTextSize16), false, std::abs(text_toolstrip_state_.style.font_size - 16.0f) < 0.01f, edit_enabled},
                        {L"Text 20 px", UiActionFromImgViewerAction(ImgViewerAction::EditTextSize20), false, std::abs(text_toolstrip_state_.style.font_size - 20.0f) < 0.01f, edit_enabled},
                        {L"Text 28 px", UiActionFromImgViewerAction(ImgViewerAction::EditTextSize28), false, std::abs(text_toolstrip_state_.style.font_size - 28.0f) < 0.01f, edit_enabled},
                        {L"Text 36 px", UiActionFromImgViewerAction(ImgViewerAction::EditTextSize36), false, std::abs(text_toolstrip_state_.style.font_size - 36.0f) < 0.01f, edit_enabled},
                    }},
                    {L"Color", kUiActionNone, false, false, edit_enabled, std::vector<MenuItem>{
                        {L"Text Red", UiActionFromImgViewerAction(ImgViewerAction::EditTextColorRed), false, SameColor(text_toolstrip_state_.style.text_color, D2D1::ColorF(D2D1::ColorF::Red)), edit_enabled},
                        {L"Text Yellow", UiActionFromImgViewerAction(ImgViewerAction::EditTextColorYellow), false, SameColor(text_toolstrip_state_.style.text_color, D2D1::ColorF(D2D1::ColorF::Yellow)), edit_enabled},
                        {L"Text Green", UiActionFromImgViewerAction(ImgViewerAction::EditTextColorGreen), false, SameColor(text_toolstrip_state_.style.text_color, D2D1::ColorF(D2D1::ColorF::Lime)), edit_enabled},
                        {L"Text Cyan", UiActionFromImgViewerAction(ImgViewerAction::EditTextColorCyan), false, SameColor(text_toolstrip_state_.style.text_color, D2D1::ColorF(D2D1::ColorF::Cyan)), edit_enabled},
                        {L"Text Blue", UiActionFromImgViewerAction(ImgViewerAction::EditTextColorBlue), false, SameColor(text_toolstrip_state_.style.text_color, D2D1::ColorF(D2D1::ColorF::DodgerBlue)), edit_enabled},
                        {L"Text Magenta", UiActionFromImgViewerAction(ImgViewerAction::EditTextColorMagenta), false, SameColor(text_toolstrip_state_.style.text_color, D2D1::ColorF(D2D1::ColorF::Magenta)), edit_enabled},
                        {L"Text White", UiActionFromImgViewerAction(ImgViewerAction::EditTextColorWhite), false, SameColor(text_toolstrip_state_.style.text_color, D2D1::ColorF(D2D1::ColorF::White)), edit_enabled},
                        {L"Text Black", UiActionFromImgViewerAction(ImgViewerAction::EditTextColorBlack), false, SameColor(text_toolstrip_state_.style.text_color, D2D1::ColorF(D2D1::ColorF::Black)), edit_enabled},
                    }},
                    {L"Background", kUiActionNone, false, false, edit_enabled, std::vector<MenuItem>{
                        {L"Transparent", UiActionFromImgViewerAction(ImgViewerAction::EditTextBackgroundTransparent), false, !text_toolstrip_state_.style.has_background, edit_enabled},
                        {L"Yellow", UiActionFromImgViewerAction(ImgViewerAction::EditTextBackgroundYellow), false, text_toolstrip_state_.style.has_background && SameColor(text_toolstrip_state_.style.background_color, D2D1::ColorF(D2D1::ColorF::Yellow, 0.82f)), edit_enabled},
                        {L"White", UiActionFromImgViewerAction(ImgViewerAction::EditTextBackgroundWhite), false, text_toolstrip_state_.style.has_background && SameColor(text_toolstrip_state_.style.background_color, D2D1::ColorF(D2D1::ColorF::White, 0.82f)), edit_enabled},
                        {L"Black", UiActionFromImgViewerAction(ImgViewerAction::EditTextBackgroundBlack), false, text_toolstrip_state_.style.has_background && SameColor(text_toolstrip_state_.style.background_color, D2D1::ColorF(D2D1::ColorF::Black, 0.82f)), edit_enabled},
                        {L"Red", UiActionFromImgViewerAction(ImgViewerAction::EditTextBackgroundRed), false, text_toolstrip_state_.style.has_background && SameColor(text_toolstrip_state_.style.background_color, D2D1::ColorF(D2D1::ColorF::Red, 0.82f)), edit_enabled},
                        {L"Blue", UiActionFromImgViewerAction(ImgViewerAction::EditTextBackgroundBlue), false, text_toolstrip_state_.style.has_background && SameColor(text_toolstrip_state_.style.background_color, D2D1::ColorF(D2D1::ColorF::DodgerBlue, 0.82f)), edit_enabled},
                    }},
                }},
                {L"Crop / Selection", kUiActionNone, false, false, edit_enabled, std::vector<MenuItem>{
                    {L"Cancel Crop", UiActionFromImgViewerAction(ImgViewerAction::EditCancelCrop), false, false, edit_toolbar_state_.tool == ImgViewerEditTool::Crop},
                    {L"Copy Pixel Selection", UiActionFromImgViewerAction(ImgViewerAction::EditCopySelection), false, false, selection_toolstrip_state_.visible},
                    {L"Mosaic Pixel Selection", UiActionFromImgViewerAction(ImgViewerAction::EditMosaicSelection), false, false, selection_toolstrip_state_.visible},
                    {L"Delete Selection", UiActionFromImgViewerAction(ImgViewerAction::EditDeleteSelection), false, false, edit_toolbar_state_.tool == ImgViewerEditTool::Select},
                }},
                {L"History", kUiActionNone, false, false, edit_enabled, std::vector<MenuItem>{
                    {L"Edit Rotate Clockwise", UiActionFromImgViewerAction(ImgViewerAction::EditRotateClockwise), false, false, edit_enabled},
                    {L"Undo Edit", UiActionFromImgViewerAction(ImgViewerAction::EditUndo), false, false, edit_enabled},
                    {L"Redo Edit", UiActionFromImgViewerAction(ImgViewerAction::EditRedo), false, false, edit_enabled},
                }},
            }},
            {L"", kUiActionNone, true},
            {L"Rotate Clockwise", UiActionFromImgViewerAction(ImgViewerAction::RotateClockwise)},
            {L"Flip Horizontal", UiActionFromImgViewerAction(ImgViewerAction::FlipHorizontal)},
            {L"Flip Vertical", UiActionFromImgViewerAction(ImgViewerAction::FlipVertical)},
            {L"", kUiActionNone, true},
            {L"Top Most", UiActionFromImgViewerAction(ImgViewerAction::ToggleTopMost), false, top_most_},
            {L"Minimize", UiActionFromImgViewerAction(ImgViewerAction::Minimize)},
            {L"Maximize or Restore", UiActionFromImgViewerAction(ImgViewerAction::ToggleMaximize)},
            {L"Close", UiActionFromImgViewerAction(ImgViewerAction::Close)},
        }));
}

bool ImgViewerUi::IsPointInCaptionDragArea(D2D1_POINT_2F point) const
{
    return titlebar_.IsPointInCaptionDragArea(*root_, point);
}

void ImgViewerUi::SetTitleText(const wchar_t* title)
{
    titlebar_.SetTitleText(title);
}

void ImgViewerUi::ShowToast(const wchar_t* text)
{
    toast_.Show(text);
}

bool ImgViewerUi::HideToast()
{
    return toast_.Hide();
}

void ImgViewerUi::SetWindowState(bool top_most, bool maximized)
{
    top_most_ = top_most;
    maximized_ = maximized;
}

void ImgViewerUi::SetColorPickerActive(bool active)
{
    color_picker_active_ = active;
    color_picker_toolstrip_state_.visible = active;
    color_picker_toolstrip_.SetState(color_picker_toolstrip_state_);
}

void ImgViewerUi::SetToolbarScalePercent(int percent)
{
    toolbar_.SetScalePercent(percent);
    color_picker_toolstrip_.SetScalePercent(percent);
    edit_toolbar_.SetScalePercent(percent);
    pen_toolstrip_.SetScalePercent(percent);
    shape_toolstrip_.SetScalePercent(percent);
    text_toolstrip_.SetScalePercent(percent);
    selection_toolstrip_.SetScalePercent(percent);
    animation_toolbar_.SetScalePercent(percent);
}

void ImgViewerUi::SetActionEnabled(UiAction action, bool enabled)
{
    if (action == UiActionFromImgViewerAction(ImgViewerAction::SaveImageAs)) {
        save_image_as_enabled_ = enabled;
    }
    if (action == UiActionFromImgViewerAction(ImgViewerAction::ShowInFileExplorer)) {
        show_in_file_explorer_enabled_ = enabled;
    }
}

const wchar_t* ImgViewerUi::ElementValue(UiElementId id) const
{
    if (color_picker_toolstrip_.IsValueElement(id)) {
        return color_picker_toolstrip_.ValueText();
    }
    return L"";
}

bool ImgViewerUi::IsElementReadOnly(UiElementId id) const
{
    return color_picker_toolstrip_.IsValueElement(id);
}

void ImgViewerUi::SetInfoPanelState(ImgViewerUiInfoPanelState state)
{
    info_panel_.SetState(std::move(state));
}

void ImgViewerUi::SetAnimationState(ImgViewerAnimationState state)
{
    animation_state_ = state;
    animation_toolbar_.SetState(state);
}

void ImgViewerUi::SetEditToolbarState(ImgViewerUiEditToolbarState state)
{
    edit_toolbar_state_ = state;
    edit_toolbar_.SetState(state);
}

void ImgViewerUi::SetColorPickerToolstripState(ImgViewerUiColorPickerToolstripState state)
{
    color_picker_toolstrip_state_ = std::move(state);
    color_picker_active_ = color_picker_toolstrip_state_.visible;
    color_picker_toolstrip_.SetState(color_picker_toolstrip_state_);
}

void ImgViewerUi::SetPenToolstripState(ImgViewerUiPenToolstripState state)
{
    pen_toolstrip_state_ = state;
    pen_toolstrip_.SetState(state);
}

void ImgViewerUi::SetShapeToolstripState(ImgViewerUiShapeToolstripState state)
{
    shape_toolstrip_state_ = state;
    shape_toolstrip_.SetState(state);
}

void ImgViewerUi::SetTextToolstripState(ImgViewerUiTextToolstripState state)
{
    text_toolstrip_state_ = std::move(state);
    text_toolstrip_.SetState(text_toolstrip_state_);
}

void ImgViewerUi::SetSelectionToolstripState(ImgViewerUiSelectionToolstripState state)
{
    selection_toolstrip_state_ = state;
    selection_toolstrip_.SetState(state);
}

const std::wstring& ImgViewerUi::SelectedTextFontFamily() const
{
    return text_toolstrip_.SelectedFontFamily();
}
