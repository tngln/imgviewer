#include "imgviewer.ui.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <d2d1helper.h>

#include "imgviewer.config.hpp"
#include "imgviewer.strings.hpp"
#include "imgviewer.ui.action.hpp"
#include "math.hpp"
#include "ui.popup.hpp"
#include "imgviewer.script_engine.hpp"
#include "imgviewer.script_ui.hpp"

namespace {

constexpr char kMainScriptRelativePath[] = "scripts/main_ui.js";
constexpr D2D1_POINT_2F kMainMenuOrigin{3.0f, 42.0f};
constexpr float kTitleBarHeight = 42.0f;

ImgViewerUi* ScriptUi(JSContext* context)
{
    return static_cast<ImgViewerUi*>(JS_GetContextOpaque(context));
}

void SetFunction(JSContext* context, JSValue object, const char* name, JSCFunction* function, int length)
{
    JS_SetPropertyStr(context, object, name, JS_NewCFunction(context, function, name, length));
}

void SetString(JSContext* context, JSValue object, const char* name, std::wstring_view value)
{
    JS_SetPropertyStr(context, object, name, JS_NewString(context, imgviewer::Utf8FromWide(value).c_str()));
}

void SetString(JSContext* context, JSValue object, const char* name, const char* value)
{
    JS_SetPropertyStr(context, object, name, JS_NewString(context, value != nullptr ? value : ""));
}

void SetBool(JSContext* context, JSValue object, const char* name, bool value)
{
    JS_SetPropertyStr(context, object, name, JS_NewBool(context, value));
}

void SetInt(JSContext* context, JSValue object, const char* name, int value)
{
    JS_SetPropertyStr(context, object, name, JS_NewInt32(context, value));
}

void SetFloat(JSContext* context, JSValue object, const char* name, float value)
{
    JS_SetPropertyStr(context, object, name, JS_NewFloat64(context, value));
}

std::string ColorHex(D2D1_COLOR_F color)
{
    const auto byte = [](float value) {
        return static_cast<unsigned int>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    char buffer[10] = {};
    std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", byte(color.r), byte(color.g), byte(color.b));
    return buffer;
}

const char* EditToolName(ImgViewerEditTool tool)
{
    switch (tool) {
    case ImgViewerEditTool::PixelSelect:
        return "pixelSelect";
    case ImgViewerEditTool::Pen:
        return "pen";
    case ImgViewerEditTool::Shape:
        return "shape";
    case ImgViewerEditTool::Text:
        return "text";
    case ImgViewerEditTool::Crop:
        return "crop";
    case ImgViewerEditTool::Select:
    default:
        return "select";
    }
}

const char* ShapeKindName(ImgViewerShapeKind kind)
{
    switch (kind) {
    case ImgViewerShapeKind::Ellipse:
        return "ellipse";
    case ImgViewerShapeKind::Line:
        return "line";
    case ImgViewerShapeKind::Arrow:
        return "arrow";
    case ImgViewerShapeKind::Rectangle:
    default:
        return "rectangle";
    }
}

JSValue MetadataRows(JSContext* context, const std::vector<ImageMetadataRow>& rows)
{
    JSValue array = JS_NewArray(context);
    uint32_t index = 0;
    for (const ImageMetadataRow& row : rows) {
        JSValue item = JS_NewObject(context);
        SetString(context, item, "label", row.label);
        SetString(context, item, "value", row.value);
        JS_SetPropertyUint32(context, array, index++, item);
    }
    return array;
}

JSValue ColorSample(JSContext* context, ImageColorSample sample)
{
    JSValue value = JS_NewObject(context);
    SetInt(context, value, "red", sample.red);
    SetInt(context, value, "green", sample.green);
    SetInt(context, value, "blue", sample.blue);
    return value;
}

std::optional<bool> OptionalBoolProperty(JSContext* context, JSValueConst object, const char* name)
{
    if (!JS_IsObject(object)) {
        return std::nullopt;
    }
    JSValue value = JS_GetPropertyStr(context, object, name);
    if (JS_IsUndefined(value)) {
        JS_FreeValue(context, value);
        return std::nullopt;
    }
    const bool result = JS_ToBool(context, value) != 0;
    JS_FreeValue(context, value);
    return result;
}

bool BoolProperty(JSContext* context, JSValueConst object, const char* name, bool fallback)
{
    if (!JS_IsObject(object)) {
        return fallback;
    }
    JSValue value = JS_GetPropertyStr(context, object, name);
    const bool result = JS_IsUndefined(value) ? fallback : JS_ToBool(context, value) != 0;
    JS_FreeValue(context, value);
    return result;
}

ImgViewerAction ActionProperty(JSContext* context, JSValueConst object)
{
    if (!JS_IsObject(object)) {
        return ImgViewerAction::None;
    }
    JSValue value = JS_GetPropertyStr(context, object, "action");
    const std::string name = imgviewer::Utf8FromValue(context, value);
    JS_FreeValue(context, value);
    return ImgViewerActionFromName(name.c_str());
}

int32_t Int32Property(JSContext* context, JSValueConst object, const char* name, int32_t fallback)
{
    if (!JS_IsObject(object)) {
        return fallback;
    }
    JSValue value = JS_GetPropertyStr(context, object, name);
    if (JS_IsUndefined(value)) {
        JS_FreeValue(context, value);
        return fallback;
    }
    int32_t result = fallback;
    JS_ToInt32(context, &result, value);
    JS_FreeValue(context, value);
    return result;
}

JSValue OverlayAction(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
{
    if (ScriptUi(context) == nullptr || argc < 1) {
        return JS_FALSE;
    }
    const ImgViewerAction action = ImgViewerActionFromName(imgviewer::Utf8FromValue(context, argv[0]).c_str());
    if (action == ImgViewerAction::None) {
        return JS_FALSE;
    }
    JSValue result = JS_NewObject(context);
    SetString(context, result, "action", ImgViewerActionName(action));
    if (argc > 1) {
        int32_t action_arg = 0;
        JS_ToInt32(context, &action_arg, argv[1]);
        SetInt(context, result, "actionArg", action_arg);
    }
    return result;
}

JSValue OverlayOpenMenu(JSContext* context, JSValueConst, int, JSValueConst*)
{
    if (ImgViewerUi* ui = ScriptUi(context)) {
        ui->RequestInvalidate();
    }
    JSValue result = JS_NewObject(context);
    SetString(context, result, "action", ImgViewerActionName(ImgViewerAction::OpenMenu));
    return result;
}

} // namespace

ImgViewerUi::ImgViewerUi(imgviewer::ScriptEngine& engine) :
    ScriptWindowRootBase(engine, kMainScriptRelativePath, "imgviewerMainUi", L"Main TypeScript UI failed")
{
    ReloadScript();
}

ImgViewerUi::~ImgViewerUi() = default;

const wchar_t* ImgViewerUi::AccessibilityName() const { return L"ImgViewer"; }

void ImgViewerUi::Render(const UiDrawContext& context)
{
    rect_ = D2D1::RectF(0.0f, 0.0f, context.viewport_size.width, context.viewport_size.height);
    if (!ready_) {
        RenderError(context);
        return;
    }

    JSContext* js_context = Context();
    JSValue app = AppObject();
    JSValue render = JS_GetPropertyStr(js_context, app, "render");
    if (!JS_IsFunction(js_context, render)) {
        JS_FreeValue(js_context, render);
        JS_FreeValue(js_context, app);
        SetError("imgviewerMainUi.render is not a function");
        RenderError(context);
        return;
    }

    JSValue canvas = imgviewer::CreateCanvasObject(js_context);
    JSValue env = imgviewer::CreateRenderEnvironment(js_context, context);
    JSValue snapshot = CreateStateObject();
    JSValue args[] = {canvas, env, snapshot};
    SetActiveDrawContext(&context);
    JSValue result = JS_Call(js_context, render, app, 3, args);
    SetActiveDrawContext(nullptr);
    JS_FreeValue(js_context, snapshot);
    JS_FreeValue(js_context, env);
    JS_FreeValue(js_context, canvas);
    JS_FreeValue(js_context, render);
    JS_FreeValue(js_context, app);

    if (JS_IsException(result)) {
        JS_FreeValue(js_context, result);
        script_context_->CaptureException();
        SetError(engine_.TakeExceptionTextUtf8());
        RenderError(context);
        return;
    }
    JS_FreeValue(js_context, result);
    if (engine_.PumpJobs() < 0) {
        SetError(engine_.TakeExceptionTextUtf8());
    }
}

UiEventResult ImgViewerUi::OnPointerEvent(const UiPointerEvent& event)
{
    if (!ready_) {
        return UiEventResult{.handled = IsOverlayPoint(event.point)};
    }
    return DispatchPointerToScript(event);
}

UiEventResult ImgViewerUi::OnKeyEvent(const UiKeyEvent& event)
{
    if (event.type == UiEventType::KeyDown && event.virtual_key == VK_F5) {
        ReloadScript();
        return UiEventResult{.handled = true, .value_changed = true};
    }
    if (!ready_) {
        return {};
    }
    return DispatchKeyToScript(event);
}

UiEventResult ImgViewerUi::OnInputEvent(const UiInputEvent& event)
{
    switch (event.type) {
    case UiEventType::TextChar:
    case UiEventType::ImeStartComposition:
    case UiEventType::ImeComposition:
    case UiEventType::ImeEndComposition:
        return ready_ ? DispatchInputToScript(event) : UiEventResult{};
    default:
        return ScriptView::OnInputEvent(event);
    }
}

bool ImgViewerUi::HandleUiAction(UiAction action, PopupHost* popup_host)
{
    if (ImgViewerActionFromUiAction(action) != ImgViewerAction::OpenMenu || popup_host == nullptr) {
        return false;
    }

    const bool edit_enabled = edit_toolbar_state_.visible;
    std::vector<MenuItem> menu_items{
        {ImgViewerString(ImgViewerStringId::OpenImage), UiActionFromImgViewerAction(ImgViewerAction::OpenImage), false, false, ActionEnabled(ImgViewerAction::OpenImage)},
        {ImgViewerString(ImgViewerStringId::CaptureRegion), UiActionFromImgViewerAction(ImgViewerAction::CaptureRegion), false, false, ActionEnabled(ImgViewerAction::CaptureRegion)},
        {ImgViewerString(ImgViewerStringId::SaveAs), UiActionFromImgViewerAction(ImgViewerAction::SaveImageAs), false, false, ActionEnabled(ImgViewerAction::SaveImageAs)},
        {ImgViewerString(ImgViewerStringId::ShowInFileExplorer), UiActionFromImgViewerAction(ImgViewerAction::ShowInFileExplorer), false, false, ActionEnabled(ImgViewerAction::ShowInFileExplorer)},
        {L"", kUiActionNone, true},
        {ImgViewerString(ImgViewerStringId::Settings), UiActionFromImgViewerAction(ImgViewerAction::OpenSettings), false, false, ActionEnabled(ImgViewerAction::OpenSettings)},
        {ImgViewerString(ImgViewerStringId::About), UiActionFromImgViewerAction(ImgViewerAction::OpenAbout), false, false, ActionEnabled(ImgViewerAction::OpenAbout)},
        {L"", kUiActionNone, true},
        {ImgViewerString(ImgViewerStringId::InfoPanel), UiActionFromImgViewerAction(ImgViewerAction::ToggleInfoPanel), false, info_panel_state_.visible, ActionEnabled(ImgViewerAction::ToggleInfoPanel)},
        {ImgViewerString(ImgViewerStringId::LoopAnimation), UiActionFromImgViewerAction(ImgViewerAction::ToggleAnimationLoop), false, animation_state_.loop, ActionEnabled(ImgViewerAction::ToggleAnimationLoop)},
        {ImgViewerString(animation_state_.playing ? ImgViewerStringId::PauseAnimation : ImgViewerStringId::PlayAnimation), UiActionFromImgViewerAction(ImgViewerAction::ToggleAnimationPlayback), false, false, ActionEnabled(ImgViewerAction::ToggleAnimationPlayback)},
        {ImgViewerString(ImgViewerStringId::PreviousAnimationFrame), UiActionFromImgViewerAction(ImgViewerAction::PreviousAnimationFrame), false, false, ActionEnabled(ImgViewerAction::PreviousAnimationFrame)},
        {ImgViewerString(ImgViewerStringId::NextAnimationFrame), UiActionFromImgViewerAction(ImgViewerAction::NextAnimationFrame), false, false, ActionEnabled(ImgViewerAction::NextAnimationFrame)},
        {L"", kUiActionNone, true},
        {ImgViewerString(ImgViewerStringId::ZoomIn), UiActionFromImgViewerAction(ImgViewerAction::ZoomIn), false, false, ActionEnabled(ImgViewerAction::ZoomIn)},
        {ImgViewerString(ImgViewerStringId::ZoomOut), UiActionFromImgViewerAction(ImgViewerAction::ZoomOut), false, false, ActionEnabled(ImgViewerAction::ZoomOut)},
        {ImgViewerString(ImgViewerStringId::FitWindow), UiActionFromImgViewerAction(ImgViewerAction::FitWindow), false, false, ActionEnabled(ImgViewerAction::FitWindow)},
        {ImgViewerString(ImgViewerStringId::ActualSize), UiActionFromImgViewerAction(ImgViewerAction::ActualSize), false, false, ActionEnabled(ImgViewerAction::ActualSize)},
        {ImgViewerString(ImgViewerStringId::ResetView), UiActionFromImgViewerAction(ImgViewerAction::ResetView), false, false, ActionEnabled(ImgViewerAction::ResetView)},
        {ImgViewerString(ImgViewerStringId::ColorPicker), UiActionFromImgViewerAction(ImgViewerAction::ToggleColorPicker), false, color_picker_active_, ActionEnabled(ImgViewerAction::ToggleColorPicker)},
        {L"", kUiActionNone, true},
        {ImgViewerString(ImgViewerStringId::EditMode), kUiActionNone, false, edit_toolbar_state_.visible, ActionEnabled(ImgViewerAction::ToggleEditMode),
            std::vector<MenuItem>{
                {ImgViewerString(ImgViewerStringId::ToggleEditMode), UiActionFromImgViewerAction(ImgViewerAction::ToggleEditMode), false, edit_toolbar_state_.visible, ActionEnabled(ImgViewerAction::ToggleEditMode)},
                {L"", kUiActionNone, true},
                {ImgViewerString(ImgViewerStringId::Tool), kUiActionNone, false, false, edit_enabled,
                    std::vector<MenuItem>{
                        {ImgViewerString(ImgViewerStringId::EditSelect), UiActionFromImgViewerAction(ImgViewerAction::EditSelect), false, edit_toolbar_state_.tool == ImgViewerEditTool::Select, edit_enabled && ActionEnabled(ImgViewerAction::EditSelect)},
                        {ImgViewerString(ImgViewerStringId::PixelSelect), UiActionFromImgViewerAction(ImgViewerAction::EditPixelSelect), false, edit_toolbar_state_.tool == ImgViewerEditTool::PixelSelect, edit_enabled && ActionEnabled(ImgViewerAction::EditPixelSelect)},
                        {ImgViewerString(ImgViewerStringId::EditPen), UiActionFromImgViewerAction(ImgViewerAction::EditPen), false, edit_toolbar_state_.tool == ImgViewerEditTool::Pen, edit_enabled && ActionEnabled(ImgViewerAction::EditPen)},
                        {ImgViewerString(ImgViewerStringId::EditShape), UiActionFromImgViewerAction(ImgViewerAction::EditShape), false, edit_toolbar_state_.tool == ImgViewerEditTool::Shape, edit_enabled && ActionEnabled(ImgViewerAction::EditShape)},
                        {ImgViewerString(ImgViewerStringId::EditText), UiActionFromImgViewerAction(ImgViewerAction::EditText), false, edit_toolbar_state_.tool == ImgViewerEditTool::Text, edit_enabled && ActionEnabled(ImgViewerAction::EditText)},
                        {ImgViewerString(ImgViewerStringId::EditCrop), UiActionFromImgViewerAction(ImgViewerAction::EditCrop), false, edit_toolbar_state_.tool == ImgViewerEditTool::Crop, edit_enabled && ActionEnabled(ImgViewerAction::EditCrop)}}}}},
        {L"", kUiActionNone, true},
        {ImgViewerString(ImgViewerStringId::RotateClockwise), UiActionFromImgViewerAction(ImgViewerAction::RotateClockwise), false, false, ActionEnabled(ImgViewerAction::RotateClockwise)},
        {ImgViewerString(ImgViewerStringId::FlipHorizontal), UiActionFromImgViewerAction(ImgViewerAction::FlipHorizontal), false, false, ActionEnabled(ImgViewerAction::FlipHorizontal)},
        {ImgViewerString(ImgViewerStringId::FlipVertical), UiActionFromImgViewerAction(ImgViewerAction::FlipVertical), false, false, ActionEnabled(ImgViewerAction::FlipVertical)},
        {L"", kUiActionNone, true},
        {ImgViewerString(ImgViewerStringId::Close), UiActionFromImgViewerAction(ImgViewerAction::Close), false, false, ActionEnabled(ImgViewerAction::Close)},
    };
#if defined(IMGVIEWER_ENABLE_DEVELOPER_WINDOW)
    menu_items.insert(menu_items.begin() + 6, MenuItem{ImgViewerString(ImgViewerStringId::Developer), UiActionFromImgViewerAction(ImgViewerAction::OpenDeveloper), false, false, ActionEnabled(ImgViewerAction::OpenDeveloper)});
#endif
    return SUCCEEDED(popup_host->OpenMenu(kMainMenuOrigin, std::move(menu_items)));
}

bool ImgViewerUi::IsPointInCaptionDragArea(D2D1_POINT_2F point) const
{
    return point.y >= rect_.top && point.y <= rect_.top + kTitleBarHeight &&
        point.x >= rect_.left + 56.0f && point.x <= rect_.right - 150.0f;
}

void ImgViewerUi::SetTitleText(const wchar_t* title)
{
    title_text_ = title != nullptr ? title : L"";
}

void ImgViewerUi::ShowToast(const wchar_t* text)
{
    toast_text_ = text != nullptr ? text : L"";
    toast_visible_ = true;
}

bool ImgViewerUi::HideToast()
{
    const bool was_visible = toast_visible_;
    toast_visible_ = false;
    return was_visible;
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
}

void ImgViewerUi::SetToolbarScalePercent(int percent)
{
    toolbar_scale_percent_ = ClampToolbarScalePercent(percent);
}

void ImgViewerUi::SetActionEnabled(UiAction action, bool enabled)
{
    if (action != kUiActionNone) {
        action_enabled_[action.value] = enabled;
    }
}

void ImgViewerUi::SetInfoPanelState(ImgViewerUiInfoPanelState state)
{
    info_panel_state_ = std::move(state);
}

void ImgViewerUi::SetAnimationState(ImgViewerAnimationState state)
{
    animation_state_ = state;
}

void ImgViewerUi::SetEditToolbarState(ImgViewerUiEditToolbarState state)
{
    edit_toolbar_state_ = state;
}

void ImgViewerUi::SetColorPickerToolstripState(ImgViewerUiColorPickerToolstripState state)
{
    color_picker_toolstrip_state_ = std::move(state);
    color_picker_active_ = color_picker_toolstrip_state_.visible;
}

void ImgViewerUi::SetPenToolstripState(ImgViewerUiPenToolstripState state)
{
    pen_toolstrip_state_ = state;
}

void ImgViewerUi::SetShapeToolstripState(ImgViewerUiShapeToolstripState state)
{
    shape_toolstrip_state_ = state;
}

void ImgViewerUi::SetTextToolstripState(ImgViewerUiTextToolstripState state)
{
    text_toolstrip_state_ = std::move(state);
    selected_text_font_family_ = text_toolstrip_state_.style.font_family;
}

void ImgViewerUi::SetSelectionToolstripState(ImgViewerUiSelectionToolstripState state)
{
    selection_toolstrip_state_ = state;
}

const std::wstring& ImgViewerUi::SelectedTextFontFamily() const
{
    return selected_text_font_family_;
}

void ImgViewerUi::BeforeReload()
{
    pending_action_ = ImgViewerAction::None;
}

void ImgViewerUi::InstallCustomGlobals(JSValue global)
{
    JSContext* context = Context();
    JSValue overlay = JS_NewObject(context);
    SetFunction(overlay, "action", OverlayAction, 1);
    SetFunction(overlay, "openMenu", OverlayOpenMenu, 0);
    SetFunction(overlay, "invalidate", imgviewer::HostInvalidate, 0);
    JS_SetPropertyStr(context, global, "overlay", overlay);
}

JSValue ImgViewerUi::CreateStateObject() const
{
    JSContext* context = Context();
    JSValue state = JS_NewObject(context);
    SetString(context, state, "title", title_text_);
    SetBool(context, state, "topMost", top_most_);
    SetBool(context, state, "maximized", maximized_);
    SetBool(context, state, "editMode", edit_toolbar_state_.visible);
    SetInt(context, state, "toolbarScalePercent", toolbar_scale_percent_);
    SetBool(context, state, "colorPickerActive", color_picker_active_);

    JSValue actions = JS_NewObject(context);
    JSValue labels = JS_NewObject(context);
    for (const ImgViewerActionInfo& action : ImgViewerActions()) {
        const char* name = ImgViewerActionName(action.action);
        JS_SetPropertyStr(context, actions, name, JS_NewBool(context, ActionEnabled(action.action)));
        SetString(context, labels, name, ImgViewerActionDisplayName(action.action));
    }
    JS_SetPropertyStr(context, state, "actionEnabled", actions);
    JS_SetPropertyStr(context, state, "actionLabels", labels);

    JSValue edit = JS_NewObject(context);
    SetBool(context, edit, "visible", edit_toolbar_state_.visible);
    SetString(context, edit, "tool", EditToolName(edit_toolbar_state_.tool));
    SetBool(context, edit, "dirty", edit_toolbar_state_.dirty);
    SetBool(context, edit, "canUndo", edit_toolbar_state_.can_undo);
    SetBool(context, edit, "canRedo", edit_toolbar_state_.can_redo);
    JS_SetPropertyStr(context, state, "editToolbar", edit);

    JSValue color_picker = JS_NewObject(context);
    SetBool(context, color_picker, "visible", color_picker_toolstrip_state_.visible);
    SetBool(context, color_picker, "hasSample", color_picker_toolstrip_state_.has_sample);
    SetString(context, color_picker, "hexText", color_picker_toolstrip_state_.hex_text);
    JS_SetPropertyStr(context, state, "colorPickerToolstrip", color_picker);

    JSValue pen = JS_NewObject(context);
    SetBool(context, pen, "visible", pen_toolstrip_state_.visible);
    SetString(context, pen, "color", ColorHex(pen_toolstrip_state_.color).c_str());
    SetFloat(context, pen, "width", pen_toolstrip_state_.width);
    JS_SetPropertyStr(context, state, "penToolstrip", pen);

    JSValue shape = JS_NewObject(context);
    SetBool(context, shape, "visible", shape_toolstrip_state_.visible);
    SetString(context, shape, "kind", ShapeKindName(shape_toolstrip_state_.kind));
    SetString(context, shape, "color", ColorHex(shape_toolstrip_state_.color).c_str());
    JS_SetPropertyStr(context, state, "shapeToolstrip", shape);

    JSValue text = JS_NewObject(context);
    SetBool(context, text, "visible", text_toolstrip_state_.visible);
    SetString(context, text, "fontFamily", text_toolstrip_state_.style.font_family);
    SetFloat(context, text, "fontSize", text_toolstrip_state_.style.font_size);
    SetString(context, text, "textColor", ColorHex(text_toolstrip_state_.style.text_color).c_str());
    SetString(context, text, "backgroundColor", ColorHex(text_toolstrip_state_.style.background_color).c_str());
    SetBool(context, text, "hasBackground", text_toolstrip_state_.style.has_background);
    JS_SetPropertyStr(context, state, "textToolstrip", text);

    JSValue selection = JS_NewObject(context);
    SetBool(context, selection, "visible", selection_toolstrip_state_.visible);
    JS_SetPropertyStr(context, state, "selectionToolstrip", selection);

    JSValue animation = JS_NewObject(context);
    SetBool(context, animation, "available", animation_state_.available);
    SetBool(context, animation, "playing", animation_state_.playing);
    SetBool(context, animation, "loop", animation_state_.loop);
    SetInt(context, animation, "currentFrame", static_cast<int>(animation_state_.current_frame));
    SetInt(context, animation, "totalFrames", static_cast<int>(animation_state_.total_frames));
    JS_SetPropertyStr(context, state, "animation", animation);

    JSValue info = JS_NewObject(context);
    SetBool(context, info, "visible", info_panel_state_.visible);
    SetBool(context, info, "hasAnalysis", info_panel_state_.has_analysis);
    SetBool(context, info, "analysisUnavailable", info_panel_state_.analysis_unavailable);
    SetString(context, info, "name", info_panel_state_.name);
    SetString(context, info, "path", info_panel_state_.path);
    SetString(context, info, "dimensions", info_panel_state_.dimensions);
    SetString(context, info, "type", info_panel_state_.type);
    SetString(context, info, "fileSize", info_panel_state_.file_size);
    SetString(context, info, "modifiedTime", info_panel_state_.modified_time);
    JS_SetPropertyStr(context, info, "colorRows", MetadataRows(context, info_panel_state_.color_rows));
    JS_SetPropertyStr(context, info, "exifRows", MetadataRows(context, info_panel_state_.exif_rows));
    JSValue analysis = JS_NewObject(context);
    SetInt(context, analysis, "sampledPixels", static_cast<int>(info_panel_state_.analysis.sampled_pixels));
    SetBool(context, analysis, "downsampled", info_panel_state_.analysis.downsampled);
    JS_SetPropertyStr(context, analysis, "average", ColorSample(context, info_panel_state_.analysis.average));
    JS_SetPropertyStr(context, analysis, "darkest", ColorSample(context, info_panel_state_.analysis.darkest));
    JS_SetPropertyStr(context, analysis, "brightest", ColorSample(context, info_panel_state_.analysis.brightest));
    JS_SetPropertyStr(context, info, "analysis", analysis);
    JS_SetPropertyStr(context, state, "infoPanel", info);

    JSValue toast = JS_NewObject(context);
    SetBool(context, toast, "visible", toast_visible_);
    SetString(context, toast, "text", toast_text_);
    JS_SetPropertyStr(context, state, "toast", toast);
    return state;
}

UiEventResult ImgViewerUi::DispatchPointerToScript(const UiPointerEvent& event)
{
    JSContext* context = Context();
    JSValue app = AppObject();
    JSValue handler = JS_GetPropertyStr(context, app, "pointer");
    if (!JS_IsFunction(context, handler)) {
        JS_FreeValue(context, handler);
        JS_FreeValue(context, app);
        return {};
    }
    JSValue js_event = imgviewer::CreatePointerEvent(context, event);
    JSValue result = JS_Call(context, handler, app, 1, &js_event);
    JS_FreeValue(context, js_event);
    JS_FreeValue(context, handler);
    JS_FreeValue(context, app);
    return FinishEventDispatch(result);
}

UiEventResult ImgViewerUi::DispatchKeyToScript(const UiKeyEvent& event)
{
    JSContext* context = Context();
    JSValue app = AppObject();
    JSValue handler = JS_GetPropertyStr(context, app, "key");
    if (!JS_IsFunction(context, handler)) {
        JS_FreeValue(context, handler);
        JS_FreeValue(context, app);
        return {};
    }
    JSValue js_event = imgviewer::CreateKeyEvent(context, event);
    JSValue result = JS_Call(context, handler, app, 1, &js_event);
    JS_FreeValue(context, js_event);
    JS_FreeValue(context, handler);
    JS_FreeValue(context, app);
    return FinishEventDispatch(result);
}

UiEventResult ImgViewerUi::DispatchInputToScript(const UiInputEvent& event)
{
    JSContext* context = Context();
    JSValue app = AppObject();
    JSValue handler = JS_GetPropertyStr(context, app, "input");
    if (!JS_IsFunction(context, handler)) {
        JS_FreeValue(context, handler);
        JS_FreeValue(context, app);
        return {};
    }
    JSValue js_event = imgviewer::CreateInputEvent(context, event);
    JSValue result = JS_Call(context, handler, app, 1, &js_event);
    JS_FreeValue(context, js_event);
    JS_FreeValue(context, handler);
    JS_FreeValue(context, app);
    return FinishEventDispatch(result);
}

UiEventResult ImgViewerUi::FinishEventDispatch(JSValue result)
{
    UiEventResult event_result{};
    JSContext* context = Context();
    if (JS_IsException(result)) {
        JS_FreeValue(context, result);
        script_context_->CaptureException();
        SetError(engine_.TakeExceptionTextUtf8());
        event_result.handled = true;
        event_result.value_changed = true;
        return event_result;
    }

    const bool handled = BoolProperty(result, "handled", false);
    const std::optional<bool> capture = OptionalBoolProperty(result, "capture");
    const bool invalidate = BoolProperty(result, "invalidate", false);
    ImgViewerAction action = ActionProperty(context, result);
    int32_t action_arg = Int32Property(context, result, "actionArg", 0);
    event_result.ime_caret_point = imgviewer::ImeCaretPointProperty(context, result);
    JS_FreeValue(context, result);

    if (pending_action_ != ImgViewerAction::None) {
        action = pending_action_;
        pending_action_ = ImgViewerAction::None;
    }
    const bool wants_reload = reload_requested_;
    const bool wants_close = close_requested_;
    const bool wants_invalidate = invalidate || invalidate_requested_ || wants_reload;
    reload_requested_ = false;
    close_requested_ = false;
    invalidate_requested_ = false;
    if (wants_reload) {
        ReloadScript();
    }

    event_result.handled = handled || capture.has_value() || action != ImgViewerAction::None || wants_close;
    if (capture.has_value()) {
        event_result.capture = *capture ? UiCaptureRequest::Capture : UiCaptureRequest::Release;
    }
    if (wants_close) {
        action = ImgViewerAction::Close;
    }
    if (action != ImgViewerAction::None) {
        event_result.action = UiAction(static_cast<int>(action), action_arg);
    }
    event_result.value_changed = wants_invalidate;
    if (engine_.PumpJobs() < 0) {
        SetError(engine_.TakeExceptionTextUtf8());
        event_result.handled = true;
        event_result.value_changed = true;
    }
    return event_result;
}

bool ImgViewerUi::ActionEnabled(UiAction action) const
{
    if (action == kUiActionNone) {
        return false;
    }
    const auto found = action_enabled_.find(action.value);
    return found == action_enabled_.end() ? true : found->second;
}

bool ImgViewerUi::ActionEnabled(ImgViewerAction action) const
{
    return ActionEnabled(UiActionFromImgViewerAction(action));
}

bool ImgViewerUi::IsOverlayPoint(D2D1_POINT_2F point) const
{
    if (point.y >= rect_.top && point.y <= rect_.top + kTitleBarHeight) {
        return true;
    }
    if (info_panel_state_.visible && point.x >= rect_.right - 340.0f && point.y >= rect_.top + kTitleBarHeight) {
        return true;
    }
    if (toast_visible_ && point.y >= rect_.top + 54.0f && point.y <= rect_.top + 96.0f) {
        return true;
    }
    const float scale = static_cast<float>(toolbar_scale_percent_) / 125.0f;
    const float toolbar_height = 48.0f * scale;
    return point.y >= rect_.bottom - toolbar_height - 18.0f;
}
