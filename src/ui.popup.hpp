#pragma once

#include <windows.h>

#include <d2d1_1.h>
#include <dcomp.h>
#include <dwrite.h>
#include <dxgi1_2.h>

#include <filesystem>
#include <memory>
#include <string>
#include <wil/com.h>

#include "ui.events.hpp"
#include "ui.graphics_device.hpp"
#include "imgviewer.script_engine.hpp"
#include "imgviewer.script_ui.hpp"

namespace imgviewer {
class ScriptEngine;
}

class PopupContent {
public:
    virtual ~PopupContent() = default;
    virtual JSValue CreateState(JSContext* context) const = 0;
    virtual void ApplyResult(JSContext* context, JSValueConst result, UiEventResult* event_result) = 0;
    virtual void OnClose() {}
};

std::unique_ptr<PopupContent> MakeJsonPopupContent(std::string state_json);

class PopupHost final : public imgviewer::ScriptUiHost {
public:
    HRESULT Initialize(HWND owner, UINT action_message, GraphicsDevice* graphics, imgviewer::ScriptEngine* script_engine);
    void SetTextFormats(IDWriteTextFormat* body_text_format, IDWriteTextFormat* icon_text_format);

    bool IsOpen() const;
    void Close();
    HRESULT OpenPopup(D2D1_POINT_2F origin, std::unique_ptr<PopupContent> content);
    UiEventResult OnInputEvent(const UiInputEvent& event);
    UiEventResult OnPointerEvent(const UiPointerEvent& event);
    UiEventResult OnKeyEvent(const UiKeyEvent& event);
    const UiDrawContext* ActiveDrawContext() const override;
    void RequestInvalidate() override;
    void RequestReload() override;
    void RequestClose() override;

private:
    friend LRESULT CALLBACK PopupWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    HRESULT LoadScript();
    HRESULT OpenScriptPopup(D2D1_POINT_2F origin);
    D2D1_SIZE_F QueryScriptContentSize();
    void RenderScriptContent(const UiDrawContext& context);
    UiEventResult DispatchScriptInput(const UiInputEvent& event);
    JSValue AppObject() const;
    JSValue CreateStateObject() const;
    HRESULT OpenNativePopup(D2D1_POINT_2F origin, D2D1_SIZE_F size);
    HRESULT ResizeNativePopupToContent(bool* resized);
    HRESULT EnsureDCompResources();
    HRESULT EnsureDCompSurface(UINT width, UINT height);
    void ResetDCompPopupResources();
    void RenderNativePopup();
    HRESULT RenderDCompPopup();
    HRESULT RenderDCompPopup(UINT width, UINT height);
    void HandlePopupResult(UiEventResult result);

    HWND owner_ = nullptr;
    UINT action_message_ = 0;
    HWND popup_hwnd_ = nullptr;
    bool native_open_ = false;
    std::unique_ptr<PopupContent> content_;
    GraphicsDevice* graphics_ = nullptr;
    imgviewer::ScriptEngine* script_engine_ = nullptr;
    std::unique_ptr<imgviewer::ScriptContext> script_context_;
    const UiDrawContext* active_draw_context_ = nullptr;
    std::filesystem::path script_path_;
    std::string error_text_;
    wil::com_ptr<ID2D1DeviceContext> d2d_context_;
    wil::com_ptr<IDCompositionDevice> dcomp_device_;
    wil::com_ptr<IDCompositionTarget> dcomp_target_;
    wil::com_ptr<IDCompositionVisual> dcomp_visual_;
    wil::com_ptr<IDCompositionSurface> dcomp_surface_;
    wil::com_ptr<IDWriteFactory> dwrite_factory_;
    wil::com_ptr<IDWriteTextFormat> menu_body_text_format_;
    wil::com_ptr<IDWriteTextFormat> menu_icon_text_format_;
    IDWriteTextFormat* body_text_format_ = nullptr;
    IDWriteTextFormat* icon_text_format_ = nullptr;
    UINT dcomp_surface_width_ = 0;
    UINT dcomp_surface_height_ = 0;
    bool invalidate_requested_ = false;
    bool close_requested_ = false;
};
