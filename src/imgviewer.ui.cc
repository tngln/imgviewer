#include "imgviewer.ui.hpp"

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include <d2d1helper.h>

#include "imgviewer.strings.hpp"
#include "imgviewer.ui.action.hpp"
#include "math.hpp"
#include "ui.popup.hpp"
#include "ui.theme.hpp"

namespace {

constexpr D2D1_POINT_2F kMainMenuOrigin{3.0f, ui_theme::metrics::kTitleBarHeight + 2.0f};

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
    animation_toolbar_.Arrange(final_rect, ActiveToolstripAnchorRect());
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
    std::vector<MenuItem> menu_items{
            {ImgViewerString(ImgViewerStringId::OpenImage), UiActionFromImgViewerAction(ImgViewerAction::OpenImage)},
            {ImgViewerString(ImgViewerStringId::CaptureRegion), UiActionFromImgViewerAction(ImgViewerAction::CaptureRegion)},
            {ImgViewerString(ImgViewerStringId::SaveAs), UiActionFromImgViewerAction(ImgViewerAction::SaveImageAs), false, false, save_image_as_enabled_},
            {ImgViewerString(ImgViewerStringId::ShowInFileExplorer), UiActionFromImgViewerAction(ImgViewerAction::ShowInFileExplorer), false, false, show_in_file_explorer_enabled_},
            {L"", kUiActionNone, true},
            {ImgViewerString(ImgViewerStringId::Settings), UiActionFromImgViewerAction(ImgViewerAction::OpenSettings)},
            {ImgViewerString(ImgViewerStringId::About), UiActionFromImgViewerAction(ImgViewerAction::OpenAbout)},
            {L"", kUiActionNone, true},
            {ImgViewerString(ImgViewerStringId::InfoPanel), UiActionFromImgViewerAction(ImgViewerAction::ToggleInfoPanel), false, info_panel_.IsVisible()},
            {ImgViewerString(ImgViewerStringId::LoopAnimation), UiActionFromImgViewerAction(ImgViewerAction::ToggleAnimationLoop), false, animation_state_.loop, animation_state_.available},
            {ImgViewerString(animation_state_.playing ? ImgViewerStringId::PauseAnimation : ImgViewerStringId::PlayAnimation), UiActionFromImgViewerAction(ImgViewerAction::ToggleAnimationPlayback), false, false, animation_state_.available},
            {ImgViewerString(ImgViewerStringId::PreviousAnimationFrame), UiActionFromImgViewerAction(ImgViewerAction::PreviousAnimationFrame), false, false, animation_state_.available},
            {ImgViewerString(ImgViewerStringId::NextAnimationFrame), UiActionFromImgViewerAction(ImgViewerAction::NextAnimationFrame), false, false, animation_state_.available},
            {L"", kUiActionNone, true},
            {ImgViewerString(ImgViewerStringId::ZoomIn), UiActionFromImgViewerAction(ImgViewerAction::ZoomIn)},
            {ImgViewerString(ImgViewerStringId::ZoomOut), UiActionFromImgViewerAction(ImgViewerAction::ZoomOut)},
            {ImgViewerString(ImgViewerStringId::FitWindow), UiActionFromImgViewerAction(ImgViewerAction::FitWindow)},
            {ImgViewerString(ImgViewerStringId::ActualSize), UiActionFromImgViewerAction(ImgViewerAction::ActualSize)},
            {ImgViewerString(ImgViewerStringId::ResetView), UiActionFromImgViewerAction(ImgViewerAction::ResetView)},
            {ImgViewerString(ImgViewerStringId::ColorPicker), UiActionFromImgViewerAction(ImgViewerAction::ToggleColorPicker), false, color_picker_active_},
            {L"", kUiActionNone, true},
            {ImgViewerString(ImgViewerStringId::EditMode), kUiActionNone, false, edit_toolbar_state_.visible, true, std::vector<MenuItem>{
                {ImgViewerString(ImgViewerStringId::ToggleEditMode), UiActionFromImgViewerAction(ImgViewerAction::ToggleEditMode), false, edit_toolbar_state_.visible},
                {L"", kUiActionNone, true},
                {ImgViewerString(ImgViewerStringId::Tool), kUiActionNone, false, false, edit_enabled, std::vector<MenuItem>{
                    {ImgViewerString(ImgViewerStringId::EditSelect), UiActionFromImgViewerAction(ImgViewerAction::EditSelect), false, edit_toolbar_state_.tool == ImgViewerEditTool::Select, edit_enabled},
                    {ImgViewerString(ImgViewerStringId::PixelSelect), UiActionFromImgViewerAction(ImgViewerAction::EditPixelSelect), false, edit_toolbar_state_.tool == ImgViewerEditTool::PixelSelect, edit_enabled},
                    {ImgViewerString(ImgViewerStringId::EditPen), UiActionFromImgViewerAction(ImgViewerAction::EditPen), false, edit_toolbar_state_.tool == ImgViewerEditTool::Pen, edit_enabled},
                    {ImgViewerString(ImgViewerStringId::EditShape), UiActionFromImgViewerAction(ImgViewerAction::EditShape), false, edit_toolbar_state_.tool == ImgViewerEditTool::Shape, edit_enabled},
                    {ImgViewerString(ImgViewerStringId::EditText), UiActionFromImgViewerAction(ImgViewerAction::EditText), false, edit_toolbar_state_.tool == ImgViewerEditTool::Text, edit_enabled},
                    {ImgViewerString(ImgViewerStringId::EditCrop), UiActionFromImgViewerAction(ImgViewerAction::EditCrop), false, edit_toolbar_state_.tool == ImgViewerEditTool::Crop, edit_enabled},
                }},
                {ImgViewerString(ImgViewerStringId::EditPen), kUiActionNone, false, edit_toolbar_state_.tool == ImgViewerEditTool::Pen, edit_enabled, std::vector<MenuItem>{
                    {ImgViewerString(ImgViewerStringId::RedPen), UiActionFromImgViewerAction(ImgViewerAction::EditPenColorRed), false, math::NearlyEqual(pen_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Red)), edit_enabled},
                    {ImgViewerString(ImgViewerStringId::YellowPen), UiActionFromImgViewerAction(ImgViewerAction::EditPenColorYellow), false, math::NearlyEqual(pen_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Yellow)), edit_enabled},
                    {ImgViewerString(ImgViewerStringId::GreenPen), UiActionFromImgViewerAction(ImgViewerAction::EditPenColorGreen), false, math::NearlyEqual(pen_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Lime)), edit_enabled},
                    {ImgViewerString(ImgViewerStringId::CyanPen), UiActionFromImgViewerAction(ImgViewerAction::EditPenColorCyan), false, math::NearlyEqual(pen_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Cyan)), edit_enabled},
                    {ImgViewerString(ImgViewerStringId::BluePen), UiActionFromImgViewerAction(ImgViewerAction::EditPenColorBlue), false, math::NearlyEqual(pen_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::DodgerBlue)), edit_enabled},
                    {ImgViewerString(ImgViewerStringId::MagentaPen), UiActionFromImgViewerAction(ImgViewerAction::EditPenColorMagenta), false, math::NearlyEqual(pen_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Magenta)), edit_enabled},
                    {ImgViewerString(ImgViewerStringId::WhitePen), UiActionFromImgViewerAction(ImgViewerAction::EditPenColorWhite), false, math::NearlyEqual(pen_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::White)), edit_enabled},
                    {ImgViewerString(ImgViewerStringId::BlackPen), UiActionFromImgViewerAction(ImgViewerAction::EditPenColorBlack), false, math::NearlyEqual(pen_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Black)), edit_enabled},
                    {L"", kUiActionNone, true},
                    {ImgViewerString(ImgViewerStringId::PenWidth2), UiActionFromImgViewerAction(ImgViewerAction::EditPenWidth2), false, std::abs(pen_toolstrip_state_.width - 2.0f) < 0.01f, edit_enabled},
                    {ImgViewerString(ImgViewerStringId::PenWidth4), UiActionFromImgViewerAction(ImgViewerAction::EditPenWidth4), false, std::abs(pen_toolstrip_state_.width - 4.0f) < 0.01f, edit_enabled},
                    {ImgViewerString(ImgViewerStringId::PenWidth8), UiActionFromImgViewerAction(ImgViewerAction::EditPenWidth8), false, std::abs(pen_toolstrip_state_.width - 8.0f) < 0.01f, edit_enabled},
                    {ImgViewerString(ImgViewerStringId::PenWidth12), UiActionFromImgViewerAction(ImgViewerAction::EditPenWidth12), false, std::abs(pen_toolstrip_state_.width - 12.0f) < 0.01f, edit_enabled},
                }},
                {ImgViewerString(ImgViewerStringId::EditShape), kUiActionNone, false, edit_toolbar_state_.tool == ImgViewerEditTool::Shape, edit_enabled, std::vector<MenuItem>{
                    {ImgViewerString(ImgViewerStringId::RectangleShape), UiActionFromImgViewerAction(ImgViewerAction::EditShapeRectangle), false, shape_toolstrip_state_.kind == ImgViewerShapeKind::Rectangle, edit_enabled},
                    {ImgViewerString(ImgViewerStringId::EllipseShape), UiActionFromImgViewerAction(ImgViewerAction::EditShapeEllipse), false, shape_toolstrip_state_.kind == ImgViewerShapeKind::Ellipse, edit_enabled},
                    {ImgViewerString(ImgViewerStringId::LineShape), UiActionFromImgViewerAction(ImgViewerAction::EditShapeLine), false, shape_toolstrip_state_.kind == ImgViewerShapeKind::Line, edit_enabled},
                    {ImgViewerString(ImgViewerStringId::ArrowShape), UiActionFromImgViewerAction(ImgViewerAction::EditShapeArrow), false, shape_toolstrip_state_.kind == ImgViewerShapeKind::Arrow, edit_enabled},
                    {L"", kUiActionNone, true},
                    {ImgViewerString(ImgViewerStringId::RedShape), UiActionFromImgViewerAction(ImgViewerAction::EditPenColorRed), false, math::NearlyEqual(shape_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Red)), edit_enabled},
                    {ImgViewerString(ImgViewerStringId::YellowShape), UiActionFromImgViewerAction(ImgViewerAction::EditPenColorYellow), false, math::NearlyEqual(shape_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Yellow)), edit_enabled},
                    {ImgViewerString(ImgViewerStringId::GreenShape), UiActionFromImgViewerAction(ImgViewerAction::EditPenColorGreen), false, math::NearlyEqual(shape_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Lime)), edit_enabled},
                    {ImgViewerString(ImgViewerStringId::CyanShape), UiActionFromImgViewerAction(ImgViewerAction::EditPenColorCyan), false, math::NearlyEqual(shape_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Cyan)), edit_enabled},
                    {ImgViewerString(ImgViewerStringId::BlueShape), UiActionFromImgViewerAction(ImgViewerAction::EditPenColorBlue), false, math::NearlyEqual(shape_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::DodgerBlue)), edit_enabled},
                    {ImgViewerString(ImgViewerStringId::MagentaShape), UiActionFromImgViewerAction(ImgViewerAction::EditPenColorMagenta), false, math::NearlyEqual(shape_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Magenta)), edit_enabled},
                    {ImgViewerString(ImgViewerStringId::WhiteShape), UiActionFromImgViewerAction(ImgViewerAction::EditPenColorWhite), false, math::NearlyEqual(shape_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::White)), edit_enabled},
                    {ImgViewerString(ImgViewerStringId::BlackShape), UiActionFromImgViewerAction(ImgViewerAction::EditPenColorBlack), false, math::NearlyEqual(shape_toolstrip_state_.color, D2D1::ColorF(D2D1::ColorF::Black)), edit_enabled},
                }},
                {ImgViewerString(ImgViewerStringId::EditText), kUiActionNone, false, edit_toolbar_state_.tool == ImgViewerEditTool::Text, edit_enabled, std::vector<MenuItem>{
                    {ImgViewerString(ImgViewerStringId::Size), kUiActionNone, false, false, edit_enabled, std::vector<MenuItem>{
                        {ImgViewerString(ImgViewerStringId::TextSize12), UiActionFromImgViewerAction(ImgViewerAction::EditTextSize12), false, std::abs(text_toolstrip_state_.style.font_size - 12.0f) < 0.01f, edit_enabled},
                        {ImgViewerString(ImgViewerStringId::TextSize16), UiActionFromImgViewerAction(ImgViewerAction::EditTextSize16), false, std::abs(text_toolstrip_state_.style.font_size - 16.0f) < 0.01f, edit_enabled},
                        {ImgViewerString(ImgViewerStringId::TextSize20), UiActionFromImgViewerAction(ImgViewerAction::EditTextSize20), false, std::abs(text_toolstrip_state_.style.font_size - 20.0f) < 0.01f, edit_enabled},
                        {ImgViewerString(ImgViewerStringId::TextSize28), UiActionFromImgViewerAction(ImgViewerAction::EditTextSize28), false, std::abs(text_toolstrip_state_.style.font_size - 28.0f) < 0.01f, edit_enabled},
                        {ImgViewerString(ImgViewerStringId::TextSize36), UiActionFromImgViewerAction(ImgViewerAction::EditTextSize36), false, std::abs(text_toolstrip_state_.style.font_size - 36.0f) < 0.01f, edit_enabled},
                    }},
                    {ImgViewerString(ImgViewerStringId::Color), kUiActionNone, false, false, edit_enabled, std::vector<MenuItem>{
                        {ImgViewerString(ImgViewerStringId::TextColorRed), UiActionFromImgViewerAction(ImgViewerAction::EditTextColorRed), false, math::NearlyEqual(text_toolstrip_state_.style.text_color, D2D1::ColorF(D2D1::ColorF::Red)), edit_enabled},
                        {ImgViewerString(ImgViewerStringId::TextColorYellow), UiActionFromImgViewerAction(ImgViewerAction::EditTextColorYellow), false, math::NearlyEqual(text_toolstrip_state_.style.text_color, D2D1::ColorF(D2D1::ColorF::Yellow)), edit_enabled},
                        {ImgViewerString(ImgViewerStringId::TextColorGreen), UiActionFromImgViewerAction(ImgViewerAction::EditTextColorGreen), false, math::NearlyEqual(text_toolstrip_state_.style.text_color, D2D1::ColorF(D2D1::ColorF::Lime)), edit_enabled},
                        {ImgViewerString(ImgViewerStringId::TextColorCyan), UiActionFromImgViewerAction(ImgViewerAction::EditTextColorCyan), false, math::NearlyEqual(text_toolstrip_state_.style.text_color, D2D1::ColorF(D2D1::ColorF::Cyan)), edit_enabled},
                        {ImgViewerString(ImgViewerStringId::TextColorBlue), UiActionFromImgViewerAction(ImgViewerAction::EditTextColorBlue), false, math::NearlyEqual(text_toolstrip_state_.style.text_color, D2D1::ColorF(D2D1::ColorF::DodgerBlue)), edit_enabled},
                        {ImgViewerString(ImgViewerStringId::TextColorMagenta), UiActionFromImgViewerAction(ImgViewerAction::EditTextColorMagenta), false, math::NearlyEqual(text_toolstrip_state_.style.text_color, D2D1::ColorF(D2D1::ColorF::Magenta)), edit_enabled},
                        {ImgViewerString(ImgViewerStringId::TextColorWhite), UiActionFromImgViewerAction(ImgViewerAction::EditTextColorWhite), false, math::NearlyEqual(text_toolstrip_state_.style.text_color, D2D1::ColorF(D2D1::ColorF::White)), edit_enabled},
                        {ImgViewerString(ImgViewerStringId::TextColorBlack), UiActionFromImgViewerAction(ImgViewerAction::EditTextColorBlack), false, math::NearlyEqual(text_toolstrip_state_.style.text_color, D2D1::ColorF(D2D1::ColorF::Black)), edit_enabled},
                    }},
                    {ImgViewerString(ImgViewerStringId::Background), kUiActionNone, false, false, edit_enabled, std::vector<MenuItem>{
                        {ImgViewerString(ImgViewerStringId::Transparent), UiActionFromImgViewerAction(ImgViewerAction::EditTextBackgroundTransparent), false, !text_toolstrip_state_.style.has_background, edit_enabled},
                        {ImgViewerString(ImgViewerStringId::Yellow), UiActionFromImgViewerAction(ImgViewerAction::EditTextBackgroundYellow), false, text_toolstrip_state_.style.has_background && math::NearlyEqual(text_toolstrip_state_.style.background_color, D2D1::ColorF(D2D1::ColorF::Yellow, 0.82f)), edit_enabled},
                        {ImgViewerString(ImgViewerStringId::White), UiActionFromImgViewerAction(ImgViewerAction::EditTextBackgroundWhite), false, text_toolstrip_state_.style.has_background && math::NearlyEqual(text_toolstrip_state_.style.background_color, D2D1::ColorF(D2D1::ColorF::White, 0.82f)), edit_enabled},
                        {ImgViewerString(ImgViewerStringId::Black), UiActionFromImgViewerAction(ImgViewerAction::EditTextBackgroundBlack), false, text_toolstrip_state_.style.has_background && math::NearlyEqual(text_toolstrip_state_.style.background_color, D2D1::ColorF(D2D1::ColorF::Black, 0.82f)), edit_enabled},
                        {ImgViewerString(ImgViewerStringId::Red), UiActionFromImgViewerAction(ImgViewerAction::EditTextBackgroundRed), false, text_toolstrip_state_.style.has_background && math::NearlyEqual(text_toolstrip_state_.style.background_color, D2D1::ColorF(D2D1::ColorF::Red, 0.82f)), edit_enabled},
                        {ImgViewerString(ImgViewerStringId::Blue), UiActionFromImgViewerAction(ImgViewerAction::EditTextBackgroundBlue), false, text_toolstrip_state_.style.has_background && math::NearlyEqual(text_toolstrip_state_.style.background_color, D2D1::ColorF(D2D1::ColorF::DodgerBlue, 0.82f)), edit_enabled},
                    }},
                }},
                {ImgViewerString(ImgViewerStringId::CropSelection), kUiActionNone, false, false, edit_enabled, std::vector<MenuItem>{
                    {ImgViewerString(ImgViewerStringId::EditCancelCrop), UiActionFromImgViewerAction(ImgViewerAction::EditCancelCrop), false, false, edit_toolbar_state_.tool == ImgViewerEditTool::Crop},
                    {ImgViewerString(ImgViewerStringId::EditCopySelection), UiActionFromImgViewerAction(ImgViewerAction::EditCopySelection), false, false, selection_toolstrip_state_.visible},
                    {ImgViewerString(ImgViewerStringId::EditMosaicSelection), UiActionFromImgViewerAction(ImgViewerAction::EditMosaicSelection), false, false, selection_toolstrip_state_.visible},
                    {ImgViewerString(ImgViewerStringId::DeleteSelection), UiActionFromImgViewerAction(ImgViewerAction::EditDeleteSelection), false, false, edit_toolbar_state_.tool == ImgViewerEditTool::Select},
                }},
                {ImgViewerString(ImgViewerStringId::History), kUiActionNone, false, false, edit_enabled, std::vector<MenuItem>{
                    {ImgViewerString(ImgViewerStringId::EditRotateClockwise), UiActionFromImgViewerAction(ImgViewerAction::EditRotateClockwise), false, false, edit_enabled},
                    {ImgViewerString(ImgViewerStringId::UndoEdit), UiActionFromImgViewerAction(ImgViewerAction::EditUndo), false, false, edit_enabled},
                    {ImgViewerString(ImgViewerStringId::RedoEdit), UiActionFromImgViewerAction(ImgViewerAction::EditRedo), false, false, edit_enabled},
                }},
            }},
            {L"", kUiActionNone, true},
            {ImgViewerString(ImgViewerStringId::RotateClockwise), UiActionFromImgViewerAction(ImgViewerAction::RotateClockwise)},
            {ImgViewerString(ImgViewerStringId::FlipHorizontal), UiActionFromImgViewerAction(ImgViewerAction::FlipHorizontal)},
            {ImgViewerString(ImgViewerStringId::FlipVertical), UiActionFromImgViewerAction(ImgViewerAction::FlipVertical)},
            {L"", kUiActionNone, true},
            {ImgViewerString(ImgViewerStringId::TopMost), UiActionFromImgViewerAction(ImgViewerAction::ToggleTopMost), false, top_most_},
            {ImgViewerString(ImgViewerStringId::Minimize), UiActionFromImgViewerAction(ImgViewerAction::Minimize)},
            {ImgViewerString(ImgViewerStringId::MaximizeOrRestore), UiActionFromImgViewerAction(ImgViewerAction::ToggleMaximize)},
            {ImgViewerString(ImgViewerStringId::Close), UiActionFromImgViewerAction(ImgViewerAction::Close)},
        };
#if defined(IMGVIEWER_ENABLE_DEVELOPER_WINDOW)
    menu_items.insert(
        menu_items.begin() + 6,
        MenuItem{ImgViewerString(ImgViewerStringId::Developer), UiActionFromImgViewerAction(ImgViewerAction::OpenDeveloper)});
#endif
    return SUCCEEDED(popup_host->OpenMenu(kMainMenuOrigin, std::move(menu_items)));
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
    if (action == kUiActionNone) {
        return;
    }

    if (action == UiActionFromImgViewerAction(ImgViewerAction::SaveImageAs)) {
        save_image_as_enabled_ = enabled;
    }
    if (action == UiActionFromImgViewerAction(ImgViewerAction::ShowInFileExplorer)) {
        show_in_file_explorer_enabled_ = enabled;
    }
    SetActionEnabledRecursive(root_.get(), action, enabled);
}

D2D1_RECT_F ImgViewerUi::ActiveToolstripAnchorRect() const
{
    if (color_picker_toolstrip_state_.visible) {
        return color_picker_toolstrip_.Rect();
    }
    if (text_toolstrip_state_.visible) {
        return text_toolstrip_.Rect();
    }
    if (pen_toolstrip_state_.visible) {
        return pen_toolstrip_.Rect();
    }
    if (shape_toolstrip_state_.visible) {
        return shape_toolstrip_.Rect();
    }
    if (selection_toolstrip_state_.visible) {
        return selection_toolstrip_.Rect();
    }
    if (edit_toolbar_state_.visible) {
        return edit_toolbar_.Rect();
    }
    return toolbar_.Rect();
}

void ImgViewerUi::SetActionEnabledRecursive(UiElement* element, UiAction action, bool enabled)
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
