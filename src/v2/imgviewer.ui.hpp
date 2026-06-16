#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include <d2d1_1.h>
#include <quickjs.h>

#include "imgviewer.action.hpp"
#include "imgviewer.ui.state.hpp"
#include "imgviewer.viewer.hpp"
#include "ui.events.hpp"
#include "ui.root.hpp"
#include "v2/imgviewer.script_ui.hpp"

namespace imgviewer::v2 {
class ScriptContext;
class ScriptEngine;
}

class ImgViewerUi final : public imgviewer::v2::ScriptUiHost, public UiRoot {
public:
    explicit ImgViewerUi(imgviewer::v2::ScriptEngine& engine);
    ~ImgViewerUi() override;

    UiElement* Root() override;
    const UiElement* Root() const override;
    const wchar_t* AccessibilityRootName() const override;
    const UiDrawContext* ActiveDrawContext() const override;
    void RequestInvalidate() override;
    void RequestReload() override;
    void RequestClose() override;

    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) override;
    void Arrange(D2D1_RECT_F final_rect) override;
    void Render(const UiDrawContext& context, UiRootState state) override;
    UiEventResult OnInputEvent(const UiInputEvent& event) override;
    UiEventResult OnPointerEvent(const UiPointerEvent& event) override;
    UiEventResult OnKeyEvent(const UiKeyEvent& event) override;
    bool HandleUiAction(UiAction action, PopupHost* popup_host) override;
    UiElementId HitTest(D2D1_POINT_2F point) const override;
    bool IsPointInCaptionDragArea(D2D1_POINT_2F point) const override;

    void SetTitleText(const wchar_t* title);
    void ShowToast(const wchar_t* text);
    bool HideToast();
    void SetWindowState(bool top_most, bool maximized);
    void SetColorPickerActive(bool active);
    void SetToolbarScalePercent(int percent);
    void SetActionEnabled(UiAction action, bool enabled);
    void SetInfoPanelState(ImgViewerUiInfoPanelState state);
    void SetAnimationState(ImgViewerAnimationState state);
    void SetEditToolbarState(ImgViewerUiEditToolbarState state);
    void SetColorPickerToolstripState(ImgViewerUiColorPickerToolstripState state);
    void SetPenToolstripState(ImgViewerUiPenToolstripState state);
    void SetShapeToolstripState(ImgViewerUiShapeToolstripState state);
    void SetTextToolstripState(ImgViewerUiTextToolstripState state);
    void SetSelectionToolstripState(ImgViewerUiSelectionToolstripState state);
    const std::wstring& SelectedTextFontFamily() const;

private:
    void ReloadScript();
    void InstallGlobals();
    void SetError(std::string text);
    void RenderError(const UiDrawContext& context) const;
    UiEventResult DispatchPointerToScript(const UiPointerEvent& event);
    UiEventResult DispatchKeyToScript(const UiKeyEvent& event);
    UiEventResult DispatchInputToScript(const UiInputEvent& event);
    UiEventResult FinishEventDispatch(JSValue result);
    JSValue AppObject() const;
    JSValue CreateStateObject() const;
    bool ActionEnabled(UiAction action) const;
    bool ActionEnabled(ImgViewerAction action) const;
    bool IsOverlayPoint(D2D1_POINT_2F point) const;

    std::unique_ptr<UiElement> root_;
    imgviewer::v2::ScriptEngine& engine_;
    std::unique_ptr<imgviewer::v2::ScriptContext> script_context_;
    const UiDrawContext* active_draw_context_ = nullptr;
    std::filesystem::path script_path_;
    D2D1_RECT_F rect_ = {};
    std::unordered_map<int, bool> action_enabled_;
    std::wstring title_text_;
    std::wstring toast_text_;
    std::wstring selected_text_font_family_ = L"Segoe UI";
    ImgViewerUiInfoPanelState info_panel_state_;
    ImgViewerAnimationState animation_state_;
    ImgViewerUiEditToolbarState edit_toolbar_state_;
    ImgViewerUiColorPickerToolstripState color_picker_toolstrip_state_;
    ImgViewerUiPenToolstripState pen_toolstrip_state_;
    ImgViewerUiShapeToolstripState shape_toolstrip_state_;
    ImgViewerUiTextToolstripState text_toolstrip_state_;
    ImgViewerUiSelectionToolstripState selection_toolstrip_state_;
    std::string error_text_;
    ImgViewerAction pending_action_ = ImgViewerAction::None;
    int toolbar_scale_percent_ = 125;
    bool ready_ = false;
    bool top_most_ = false;
    bool maximized_ = false;
    bool color_picker_active_ = false;
    bool toast_visible_ = false;
    bool invalidate_requested_ = false;
    bool reload_requested_ = false;
    bool close_requested_ = false;
};
