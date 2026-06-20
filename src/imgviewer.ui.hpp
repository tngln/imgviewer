#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <d2d1_1.h>
#include <quickjs.h>

#include "imgviewer.action.hpp"
#include "imgviewer.ui.state.hpp"
#include "imgviewer.viewer.hpp"
#include "script.view.hpp"
#include "ui.events.hpp"
#include "imgviewer.script_ui.hpp"
#include "imgviewer.script_window_root.hpp"

class ImgViewerUi final : public imgviewer::ScriptWindowRootBase {
public:
    explicit ImgViewerUi(script::QuickJsRuntime& engine);
    ~ImgViewerUi() override;

    const wchar_t* AccessibilityName() const override;

    void Render(const UiDrawContext& context) override;
    UiEventResult OnInputEvent(const UiInputEvent& event) override;
    UiEventResult OnPointerEvent(const UiPointerEvent& event) override;
    UiEventResult OnKeyEvent(const UiKeyEvent& event) override;
    bool IsPointInCaptionDragArea(D2D1_POINT_2F point) const override;

    void SetTitleText(const wchar_t* title);
    UiEventResult ShowToast(HWND hwnd, const wchar_t* text);
    void SetWindowState(bool top_most, bool maximized);
    void SetToolbarScalePercent(int percent);
    void SetEdgeClickNavigationState(bool enabled, int zone_percent);
    void SetActionEnabled(UiAction action, bool enabled);
    void SetInfoPanelState(ImgViewerUiInfoPanelState state);
    void SetAnimationState(ImgViewerAnimationState state);
    void SetEditToolbarState(ImgViewerUiEditToolbarState state);
    void SetPenToolstripState(ImgViewerUiPenToolstripState state);
    void SetShapeToolstripState(ImgViewerUiShapeToolstripState state);
    void SetTextToolstripState(ImgViewerUiTextToolstripState state);
    void SetSelectionToolstripState(ImgViewerUiSelectionToolstripState state);
    const std::wstring& SelectedTextFontFamily() const;
    UiEventResult DispatchLocalActionToScript(std::string_view action, int32_t action_arg = 0, HWND hwnd = nullptr);
    HWND ActiveEventHwnd() const;

private:
    void BeforeReload() override;
    void InstallCustomGlobals(JSValue global) override;
    UiEventResult DispatchPointerToScript(const UiPointerEvent& event, HWND hwnd = nullptr);
    UiEventResult DispatchKeyToScript(const UiKeyEvent& event, HWND hwnd = nullptr);
    UiEventResult DispatchInputToScript(const UiInputEvent& event);
    UiEventResult FinishEventDispatch(JSValue result) override;
    JSValue CreateStateObject() const;
    bool ActionEnabled(UiAction action) const;
    bool ActionEnabled(ImgViewerAction action) const;
    bool IsOverlayPoint(D2D1_POINT_2F point) const;

    D2D1_RECT_F rect_ = {};
    std::vector<D2D1_RECT_F> caption_drag_rects_;
    std::unordered_map<int, bool> action_enabled_;
    std::wstring title_text_;
    std::wstring selected_text_font_family_ = L"Segoe UI";
    ImgViewerUiInfoPanelState info_panel_state_;
    ImgViewerAnimationState animation_state_;
    ImgViewerUiEditToolbarState edit_toolbar_state_;
    ImgViewerUiPenToolstripState pen_toolstrip_state_;
    ImgViewerUiShapeToolstripState shape_toolstrip_state_;
    ImgViewerUiTextToolstripState text_toolstrip_state_;
    ImgViewerUiSelectionToolstripState selection_toolstrip_state_;
    ImgViewerAction pending_action_ = ImgViewerAction::None;
    int toolbar_scale_percent_ = 125;
    int edge_click_navigation_zone_percent_ = 10;
    HWND active_event_hwnd_ = nullptr;
    bool top_most_ = false;
    bool maximized_ = false;
    bool edge_click_navigation_ = false;
};
