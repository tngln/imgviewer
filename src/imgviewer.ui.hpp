#pragma once

#include <memory>
#include <string>

#include <d2d1_1.h>
#include <dwrite.h>

#include "ui.element.hpp"
#include "ui.events.hpp"
#include "ui.root.hpp"

#include "imgviewer.ui.animation_toolbar.hpp"
#include "imgviewer.edit.hpp"
#include "imgviewer.ui.edit_toolbar.hpp"
#include "imgviewer.ui.info_panel.hpp"
#include "imgviewer.ui.pen_toolstrip.hpp"
#include "imgviewer.ui.selection_toolstrip.hpp"
#include "imgviewer.ui.text_toolstrip.hpp"
#include "imgviewer.ui.titlebar.hpp"
#include "imgviewer.ui.toolbar.hpp"
#include "ui.toast.hpp"

class ImgViewerUi final : public UiRoot {
public:
    ImgViewerUi();

    UiElement* Root() override;
    const UiElement* Root() const override;
    const wchar_t* AccessibilityRootName() const override;

    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) override;
    void Arrange(D2D1_RECT_F final_rect) override;
    void Render(
        const UiDrawContext& context,
        UiRootState state) override;
    UiEventResult OnPointerEvent(const UiPointerEvent& event) override;
    UiEventResult OnKeyEvent(const UiKeyEvent& event) override;
    bool HandleUiAction(UiAction action, PopupHost* popup_host) override;
    bool IsPointInCaptionDragArea(D2D1_POINT_2F point) const override;
    void SetTitleText(const wchar_t* title) override;
    void ShowToast(const wchar_t* text) override;
    bool HideToast() override;
    void SetWindowState(bool top_most, bool maximized) override;
    void SetColorPickerActive(bool active) override;
    void SetToolbarScalePercent(int percent) override;
    void SetActionEnabled(UiAction action, bool enabled) override;
    void SetInfoPanelState(ImgViewerUiInfoPanelState state);
    void SetAnimationState(ImgViewerAnimationState state);
    void SetEditToolbarState(ImgViewerUiEditToolbarState state);
    void SetPenToolstripState(ImgViewerUiPenToolstripState state);
    void SetTextToolstripState(ImgViewerUiTextToolstripState state);
    void SetSelectionToolstripState(ImgViewerUiSelectionToolstripState state);
    const std::wstring& SelectedTextFontFamily() const;

private:
    std::unique_ptr<UiElement> root_;
    ImgViewerUiTitleBar titlebar_;
    ImgViewerUiToolbar toolbar_;
    ImgViewerUiEditToolbar edit_toolbar_;
    ImgViewerUiPenToolstrip pen_toolstrip_;
    ImgViewerUiTextToolstrip text_toolstrip_;
    ImgViewerUiSelectionToolstrip selection_toolstrip_;
    ImgViewerUiAnimationToolbar animation_toolbar_;
    ImgViewerUiInfoPanel info_panel_;
    UiToast toast_;
    bool top_most_ = false;
    bool maximized_ = false;
    bool color_picker_active_ = false;
    bool save_image_as_enabled_ = false;
    bool show_in_file_explorer_enabled_ = false;
    ImgViewerUiEditToolbarState edit_toolbar_state_;
    ImgViewerUiPenToolstripState pen_toolstrip_state_;
    ImgViewerUiTextToolstripState text_toolstrip_state_;
    ImgViewerUiSelectionToolstripState selection_toolstrip_state_;
    ImgViewerAnimationState animation_state_;
};
