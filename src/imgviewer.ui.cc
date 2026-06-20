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
#include "imgviewer.script_ui.hpp"
#include "script.quickjs_helper.hpp"

namespace {

constexpr char kMainScriptRelativePath[] = "scripts/main_ui.js";

using imgviewer::ActionProperty;

ImgViewerUi* ScriptUi(JSContext* context)
{
    return static_cast<ImgViewerUi*>(JS_GetContextOpaque(context));
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
        script::ObjectBuilder item(context);
        item.Set("label", row.label)
            .Set("value", row.value);
        JS_SetPropertyUint32(context, array, index++, item.Release());
    }
    return array;
}

JSValue ColorSample(JSContext* context, ImageColorSample sample)
{
    script::ObjectBuilder value(context);
    value.Set("red", static_cast<int32_t>(sample.red))
        .Set("green", static_cast<int32_t>(sample.green))
        .Set("blue", static_cast<int32_t>(sample.blue));
    return value.Release();
}

std::optional<std::string> JsonStringify(JSContext* context, JSValueConst value)
{
    JSValue json = JS_JSONStringify(context, value, JS_UNDEFINED, JS_UNDEFINED);
    if (JS_IsException(json)) {
        JS_FreeValue(context, json);
        return std::nullopt;
    }
    std::string text = script::ToStringUtf8(context, json);
    JS_FreeValue(context, json);
    return text;
}

std::optional<UiPopupRequest> PopupRequestProperty(JSContext* context, JSValueConst object, bool* failed)
{
    if (!JS_IsObject(object)) {
        return std::nullopt;
    }

    JSValue popup = JS_GetPropertyStr(context, object, "popup");
    if (JS_IsUndefined(popup) || JS_IsNull(popup)) {
        JS_FreeValue(context, popup);
        return std::nullopt;
    }

    JSValue state = JS_GetPropertyStr(context, popup, "state");
    std::optional<std::string> state_json = JsonStringify(context, state);
    JS_FreeValue(context, state);
    if (!state_json.has_value()) {
        JS_FreeValue(context, popup);
        if (failed != nullptr) {
            *failed = true;
        }
        return std::nullopt;
    }

    UiPopupRequest request{
        .origin = D2D1::Point2F(script::FloatProperty(context, popup, "x", 0.0f), script::FloatProperty(context, popup, "y", 0.0f)),
        .state_json = std::move(state_json.value()),
    };
    JS_FreeValue(context, popup);
    return request;
}

std::vector<D2D1_RECT_F> CaptionDragRectsProperty(JSContext* context, JSValueConst object)
{
    std::vector<D2D1_RECT_F> rects;
    if (!JS_IsObject(object)) {
        return rects;
    }

    JSValue array = JS_GetPropertyStr(context, object, "captionDragRects");
    if (!JS_IsArray(array)) {
        JS_FreeValue(context, array);
        return rects;
    }

    JSValue length_value = JS_GetPropertyStr(context, array, "length");
    uint32_t length = 0;
    if (JS_ToUint32(context, &length, length_value) != 0) {
        JS_FreeValue(context, length_value);
        JS_FreeValue(context, array);
        return {};
    }
    JS_FreeValue(context, length_value);

    rects.reserve(length);
    for (uint32_t index = 0; index < length; ++index) {
        JSValue item = JS_GetPropertyUint32(context, array, index);
        if (JS_IsObject(item)) {
            const float x = script::FloatProperty(context, item, "x", 0.0f);
            const float y = script::FloatProperty(context, item, "y", 0.0f);
            const float width = script::FloatProperty(context, item, "width", 0.0f);
            const float height = script::FloatProperty(context, item, "height", 0.0f);
            if (width > 0.0f && height > 0.0f) {
                rects.push_back(D2D1::RectF(x, y, x + width, y + height));
            }
        }
        JS_FreeValue(context, item);
    }

    JS_FreeValue(context, array);
    return rects;
}

JSValue OverlayAction(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
{
    if (ScriptUi(context) == nullptr || argc < 1) {
        return JS_FALSE;
    }
    const ImgViewerAction action = ImgViewerActionFromName(script::ToStringUtf8(context, argv[0]).c_str());
    if (action == ImgViewerAction::None) {
        return JS_FALSE;
    }
    JSValue result = JS_NewObject(context);
    script::SetString(context, result, "action", ImgViewerActionName(action));
    if (argc > 1) {
        int32_t action_arg = 0;
        JS_ToInt32(context, &action_arg, argv[1]);
        script::SetInt(context, result, "actionArg", action_arg);
    }
    return result;
}

JSValue OverlayPopup(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
{
    if (ScriptUi(context) == nullptr || argc < 3) {
        return JS_FALSE;
    }

    double x = 0.0;
    double y = 0.0;
    JS_ToFloat64(context, &x, argv[0]);
    JS_ToFloat64(context, &y, argv[1]);

    JSValue result = JS_NewObject(context);
    script::SetBool(context, result, "handled", true);
    JSValue popup = JS_NewObject(context);
    script::SetFloat(context, popup, "x", static_cast<float>(x));
    script::SetFloat(context, popup, "y", static_cast<float>(y));
    JS_SetPropertyStr(context, popup, "state", JS_DupValue(context, argv[2]));
    JS_SetPropertyStr(context, result, "popup", popup);
    return result;
}

} // namespace

ImgViewerUi::ImgViewerUi(script::QuickJsRuntime& engine) :
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
        caption_drag_rects_.clear();
        RenderError(context);
        return;
    }

    JSContext* js_context = Context();
    JSValue app = AppObject();
    JSValue render = JS_GetPropertyStr(js_context, app, "render");
    if (!JS_IsFunction(js_context, render)) {
        JS_FreeValue(js_context, render);
        JS_FreeValue(js_context, app);
        caption_drag_rects_.clear();
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
        caption_drag_rects_.clear();
        script_context_->CaptureException();
        SetError(engine_.TakeExceptionTextUtf8());
        RenderError(context);
        return;
    }
    caption_drag_rects_ = CaptionDragRectsProperty(js_context, result);
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

bool ImgViewerUi::IsPointInCaptionDragArea(D2D1_POINT_2F point) const
{
    return std::any_of(caption_drag_rects_.begin(), caption_drag_rects_.end(), [&](D2D1_RECT_F rect) {
        return point.x >= rect.left && point.x <= rect.right &&
            point.y >= rect.top && point.y <= rect.bottom;
    });
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

void ImgViewerUi::SetEdgeClickNavigationState(bool enabled, int zone_percent)
{
    edge_click_navigation_ = enabled;
    edge_click_navigation_zone_percent_ = ClampEdgeClickNavigationZonePercent(zone_percent);
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
    caption_drag_rects_.clear();
}

void ImgViewerUi::InstallCustomGlobals(JSValue global)
{
    JSContext* context = Context();
    script::ObjectBuilder overlay(context);
    overlay.SetFunction("action", OverlayAction, 1)
        .SetFunction("popup", OverlayPopup, 3)
        .SetFunction("invalidate", imgviewer::HostInvalidate, 0);
    JS_SetPropertyStr(context, global, "overlay", overlay.Release());
}

JSValue ImgViewerUi::CreateStateObject() const
{
    JSContext* context = Context();
    script::ObjectBuilder state(context);
    state.Set("title", title_text_)
        .Set("topMost", top_most_)
        .Set("maximized", maximized_)
        .Set("editMode", edit_toolbar_state_.visible)
        .Set("toolbarScalePercent", toolbar_scale_percent_)
        .Set("edgeClickNavigation", edge_click_navigation_)
        .Set("edgeClickNavigationZonePercent", edge_click_navigation_zone_percent_)
        .Set("colorPickerActive", color_picker_active_);

    script::ObjectBuilder actions(context);
    script::ObjectBuilder labels(context);
    for (const ImgViewerActionInfo& action : ImgViewerActions()) {
        const char* name = ImgViewerActionName(action.action);
        actions.Set(name, ActionEnabled(action.action));
        labels.Set(name, ImgViewerActionDisplayName(action.action));
    }
    state.SetValue("actionEnabled", actions.Release())
        .SetValue("actionLabels", labels.Release());

    state.SetObject("editToolbar", [&](script::ObjectBuilder& edit) {
        edit.Set("visible", edit_toolbar_state_.visible)
            .Set("tool", EditToolName(edit_toolbar_state_.tool))
            .Set("dirty", edit_toolbar_state_.dirty)
            .Set("canUndo", edit_toolbar_state_.can_undo)
            .Set("canRedo", edit_toolbar_state_.can_redo);
    });

    state.SetObject("colorPickerToolstrip", [&](script::ObjectBuilder& color_picker) {
        color_picker.Set("visible", color_picker_toolstrip_state_.visible)
            .Set("hasSample", color_picker_toolstrip_state_.has_sample)
            .Set("hexText", color_picker_toolstrip_state_.hex_text);
    });

    state.SetObject("penToolstrip", [&](script::ObjectBuilder& pen) {
        pen.Set("visible", pen_toolstrip_state_.visible)
            .Set("color", ColorHex(pen_toolstrip_state_.color))
            .Set("width", pen_toolstrip_state_.width);
    });

    state.SetObject("shapeToolstrip", [&](script::ObjectBuilder& shape) {
        shape.Set("visible", shape_toolstrip_state_.visible)
            .Set("kind", ShapeKindName(shape_toolstrip_state_.kind))
            .Set("color", ColorHex(shape_toolstrip_state_.color));
    });

    state.SetObject("textToolstrip", [&](script::ObjectBuilder& text) {
        text.Set("visible", text_toolstrip_state_.visible)
            .Set("fontFamily", text_toolstrip_state_.style.font_family)
            .Set("fontSize", text_toolstrip_state_.style.font_size)
            .Set("textColor", ColorHex(text_toolstrip_state_.style.text_color))
            .Set("backgroundColor", ColorHex(text_toolstrip_state_.style.background_color))
            .Set("hasBackground", text_toolstrip_state_.style.has_background);
    });

    state.SetObject("selectionToolstrip", [&](script::ObjectBuilder& selection) {
        selection.Set("visible", selection_toolstrip_state_.visible);
    });

    state.SetObject("animation", [&](script::ObjectBuilder& animation) {
        animation.Set("available", animation_state_.available)
            .Set("playing", animation_state_.playing)
            .Set("loop", animation_state_.loop)
            .Set("currentFrame", static_cast<int32_t>(animation_state_.current_frame))
            .Set("totalFrames", static_cast<int32_t>(animation_state_.total_frames));
    });

    state.SetObject("infoPanel", [&](script::ObjectBuilder& info) {
        info.Set("visible", info_panel_state_.visible)
            .Set("hasAnalysis", info_panel_state_.has_analysis)
            .Set("analysisUnavailable", info_panel_state_.analysis_unavailable)
            .Set("name", info_panel_state_.name)
            .Set("path", info_panel_state_.path)
            .Set("dimensions", info_panel_state_.dimensions)
            .Set("type", info_panel_state_.type)
            .Set("fileSize", info_panel_state_.file_size)
            .Set("modifiedTime", info_panel_state_.modified_time)
            .SetValue("colorRows", MetadataRows(context, info_panel_state_.color_rows))
            .SetValue("exifRows", MetadataRows(context, info_panel_state_.exif_rows));
        info.SetObject("analysis", [&](script::ObjectBuilder& analysis) {
            analysis.Set("sampledPixels", static_cast<int32_t>(info_panel_state_.analysis.sampled_pixels))
                .Set("downsampled", info_panel_state_.analysis.downsampled)
                .SetValue("average", ColorSample(context, info_panel_state_.analysis.average))
                .SetValue("darkest", ColorSample(context, info_panel_state_.analysis.darkest))
                .SetValue("brightest", ColorSample(context, info_panel_state_.analysis.brightest));
        });
    });

    state.SetObject("toast", [&](script::ObjectBuilder& toast) {
        toast.Set("visible", toast_visible_)
            .Set("text", toast_text_);
    });
    return state.Release();
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

    const bool handled = script::BoolProperty(context, result, "handled", false);
    const std::optional<bool> capture = script::OptionalBoolProperty(context, result, "capture");
    const bool invalidate = script::BoolProperty(context, result, "invalidate", false);
    UiAction action = ActionProperty(context, result);
    event_result.ime_caret_point = imgviewer::ImeCaretPointProperty(context, result);
    bool popup_failed = false;
    event_result.popup = PopupRequestProperty(context, result, &popup_failed);
    if (popup_failed) {
        JS_FreeValue(context, result);
        script_context_->CaptureException();
        SetError(engine_.TakeExceptionTextUtf8());
        event_result.handled = true;
        event_result.value_changed = true;
        return event_result;
    }
    JS_FreeValue(context, result);

    if (pending_action_ != ImgViewerAction::None) {
        action = UiAction(static_cast<int>(pending_action_), action.arg);
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

    event_result.handled = handled || capture.has_value() || action != ImgViewerAction::None || wants_close || event_result.popup.has_value();
    if (capture.has_value()) {
        event_result.capture = *capture ? UiCaptureRequest::Capture : UiCaptureRequest::Release;
    }
    if (wants_close) {
        action = UiAction(static_cast<int>(ImgViewerAction::Close), action.arg);
    }
    if (action != kUiActionNone) {
        event_result.action = action;
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
    if (IsPointInCaptionDragArea(point)) {
        return true;
    }
    if (info_panel_state_.visible && point.x >= rect_.right - 340.0f) {
        return true;
    }
    if (toast_visible_ && point.y >= rect_.top + 54.0f && point.y <= rect_.top + 96.0f) {
        return true;
    }
    const float scale = static_cast<float>(toolbar_scale_percent_) / 125.0f;
    const float toolbar_height = 48.0f * scale;
    return point.y >= rect_.bottom - toolbar_height - 18.0f;
}
