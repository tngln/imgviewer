#pragma once

#include <windows.h>

#include <d2d1.h>
#include <dwrite.h>

#include <wil/com.h>

#include "ui.events.hpp"
#include "ui.menu.hpp"

class PopupHost final {
public:
    HRESULT Initialize(HWND owner, ID2D1Factory* d2d_factory, IDWriteFactory* dwrite_factory);
    void SetTextFormats(IDWriteTextFormat* body_text_format, IDWriteTextFormat* icon_text_format);

    bool IsOpen() const;
    void Close();
    HRESULT OpenMenu(D2D1_POINT_2F origin, std::vector<MenuItem> items);
    void Draw(const UiDrawContext& context) const;
    UiEventResult OnInputEvent(const UiInputEvent& event);
    UiEventResult OnPointerEvent(const UiPointerEvent& event);
    UiEventResult OnKeyEvent(const UiKeyEvent& event);
    bool Contains(D2D1_POINT_2F point) const;

private:
    friend LRESULT CALLBACK PopupWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    bool ShouldUseNativeWindow(D2D1_RECT_F bounds) const;
    HRESULT OpenNativePopup(const MenuOverlay& menu);
    HRESULT EnsureNativeRenderTarget();
    void RenderNativePopup();
    void ForwardAction(ImgViewerAction action);

    HWND owner_ = nullptr;
    HWND popup_hwnd_ = nullptr;
    MenuOverlay menu_;
    bool native_open_ = false;
    wil::com_ptr<ID2D1Factory> d2d_factory_;
    wil::com_ptr<IDWriteFactory> dwrite_factory_;
    wil::com_ptr<ID2D1HwndRenderTarget> native_render_target_;
    IDWriteTextFormat* body_text_format_ = nullptr;
    IDWriteTextFormat* icon_text_format_ = nullptr;
};
