#pragma once

#include <memory>

#include <d2d1_1.h>
#include <dwrite.h>

#include "ui.element.hpp"
#include "ui.events.hpp"
#include "ui.root.hpp"

#include "imgviewer.ui.info_panel.hpp"
#include "imgviewer.ui.titlebar.hpp"
#include "imgviewer.ui.toolbar.hpp"
#include "ui.toast.hpp"
#include "ui.menu.hpp"

class ImgViewerUi final : public UiRoot {
public:
    ImgViewerUi();

    UiElement* Root() override;
    const UiElement* Root() const override;
    const wchar_t* AccessibilityRootName() const override;

    void Draw(
        const UiDrawContext& context,
        UiRootState state) override;
    UiEventResult OnPointerEvent(const UiPointerEvent& event) override;
    UiEventResult OnKeyEvent(const UiKeyEvent& event) override;
    bool HandleUiAction(UiAction action) override;
    bool IsPointInCaptionDragArea(D2D1_POINT_2F point) const override;
    void SetTitleText(const wchar_t* title) override;
    void ShowToast(const wchar_t* text) override;
    bool HideToast() override;
    void SetWindowState(bool top_most, bool maximized) override;
    void SetColorPickerActive(bool active) override;
    void SetToolbarScalePercent(int percent) override;
    void SetActionEnabled(UiAction action, bool enabled) override;
    void SetInfoPanelState(ImgViewerUiInfoPanelState state);

private:
    void Layout(D2D1_SIZE_F viewport_size);

    std::unique_ptr<UiElement> root_;
    ImgViewerUiTitleBar titlebar_;
    ImgViewerUiToolbar toolbar_;
    ImgViewerUiInfoPanel info_panel_;
    UiToast toast_;
    bool top_most_ = false;
    bool maximized_ = false;
    bool color_picker_active_ = false;
    bool save_image_as_enabled_ = false;
    MenuOverlay menu_;
};
