#pragma once

#include <windows.h>

#include <d2d1.h>
#include <dwrite.h>

#include <memory>
#include <wil/com.h>

#include "ui.events.hpp"
#include "ui.menu.hpp"

class UiPopupContent {
public:
    virtual ~UiPopupContent() = default;

    virtual D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const = 0;
    virtual float CornerRadius() const { return 0.0f; }
    virtual void Render(const UiDrawContext& context) const = 0;
    virtual UiEventResult OnInputEvent(const UiInputEvent& event) = 0;
    virtual void OnClosed() {}
};

class PopupHost final {
public:
    HRESULT Initialize(HWND owner, UINT action_message, ID2D1Factory* d2d_factory, IDWriteFactory* dwrite_factory);
    void SetTextFormats(IDWriteTextFormat* body_text_format, IDWriteTextFormat* icon_text_format);

    bool IsOpen() const;
    void Close();
    HRESULT Open(D2D1_POINT_2F origin, std::unique_ptr<UiPopupContent> content);
    HRESULT OpenMenu(D2D1_POINT_2F origin, std::vector<MenuItem> items);
    void Render(const UiDrawContext& context) const;
    UiEventResult OnInputEvent(const UiInputEvent& event);
    UiEventResult OnPointerEvent(const UiPointerEvent& event);
    UiEventResult OnKeyEvent(const UiKeyEvent& event);
    bool Contains(D2D1_POINT_2F point) const;

private:
    friend LRESULT CALLBACK PopupWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    HRESULT OpenNativePopup(D2D1_POINT_2F origin, D2D1_SIZE_F size);
    HRESULT EnsureNativeRenderTarget();
    void RenderNativePopup();
    void HandlePopupResult(UiEventResult result);
    void ForwardAction(UiAction action, UiElementId effect_target);
    IDWriteTextFormat* MenuBodyTextFormat() const;
    IDWriteTextFormat* MenuIconTextFormat() const;

    HWND owner_ = nullptr;
    UINT action_message_ = 0;
    HWND popup_hwnd_ = nullptr;
    bool native_open_ = false;
    std::unique_ptr<UiPopupContent> content_;
    wil::com_ptr<ID2D1Factory> d2d_factory_;
    wil::com_ptr<IDWriteFactory> dwrite_factory_;
    wil::com_ptr<ID2D1HwndRenderTarget> native_render_target_;
    wil::com_ptr<IDWriteTextFormat> menu_body_text_format_;
    wil::com_ptr<IDWriteTextFormat> menu_icon_text_format_;
    IDWriteTextFormat* body_text_format_ = nullptr;
    IDWriteTextFormat* icon_text_format_ = nullptr;
};
