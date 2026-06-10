#pragma once

#include <memory>

#include <d2d1_1.h>
#include <dcomp.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <imm.h>
#include <ole2.h>
#include <UIAutomationCore.h>
#include <wil/com.h>

#include "ui.hpp"
#include "ui.graphics_device.hpp"
#include "ui.popup.hpp"
#include "win32.window.hpp"

class UiWindowHost;

class UiWindowDelegate {
public:
    virtual ~UiWindowDelegate() = default;

    virtual HRESULT OnCreate(UiWindowHost&) { return S_OK; }
    virtual void OnDestroy(UiWindowHost&) {}
    virtual bool OnUiAction(UiWindowHost&, UiAction) { return false; }
    virtual void OnUiValueChanged(UiWindowHost&, UiEventResult) {}
    virtual win32::WindowMessageResult OnUnhandledMessage(
        UiWindowHost&,
        UINT,
        WPARAM,
        LPARAM)
    {
        return win32::WindowMessageResult::Unhandled();
    }
};

struct UiWindowOptions final {
    win32::NativeWindowOptions native;
    UINT action_message = 0;
    UINT_PTR caret_timer_id = 1;
    float body_font_size = 8.5f;
    float icon_font_size = 10.0f;
    bool enable_accessibility = true;
    bool enable_popup = true;
    bool enable_ime = true;
};

class UiWindowHost final : public win32::NativeWindowDelegate {
public:
    UiWindowHost() = default;
    UiWindowHost(const UiWindowHost&) = delete;
    UiWindowHost& operator=(const UiWindowHost&) = delete;
    ~UiWindowHost() override = default;

    HRESULT Create(
        UiWindowOptions options,
        std::unique_ptr<UiRoot> root,
        UiWindowDelegate* delegate,
        GraphicsDevice* graphics);
    void ResetRoot(std::unique_ptr<UiRoot> root);
    void Invalidate();
    void Close();
    HWND Hwnd() const;
    win32::NativeWindow& Window();
    UiController& Ui();
    PopupHost& Popup();
    IDWriteFactory* DWriteFactory() const;
    IDWriteTextFormat* BodyTextFormat() const;
    IDWriteTextFormat* IconTextFormat() const;

private:
    win32::WindowMessageResult OnWindowMessage(
        win32::NativeWindow& window,
        UINT message,
        WPARAM wparam,
        LPARAM lparam) override;

    HRESULT InitializeRenderResources();
    HRESULT EnsureDCompSurface();
    void Render();
    void HandleUiResult(UiEventResult result);
    bool ExecuteAction(UiAction action);
    UiModifiers CurrentModifiers() const;
    void SyncCaretTimer();
    void PositionIme();
    D2D1_POINT_2F CaretPoint() const;
    bool IsFocusedTextElement() const;
    std::wstring ImeCompositionString(LPARAM lparam) const;
    UiEventResult DispatchInputEvent(const UiInputEvent& event);

    UiWindowOptions options_ = {};
    UiWindowDelegate* delegate_ = nullptr;
    GraphicsDevice* graphics_ = nullptr;
    win32::NativeWindow window_;
    UiController ui_{nullptr};
    PopupHost popup_;
    wil::com_ptr<ID2D1DeviceContext> d2d_context_;
    wil::com_ptr<IDCompositionDevice> dcomp_device_;
    wil::com_ptr<IDCompositionTarget> dcomp_target_;
    wil::com_ptr<IDCompositionVisual> dcomp_visual_;
    wil::com_ptr<IDCompositionSurface> dcomp_surface_;
    wil::com_ptr<IDWriteFactory> dwrite_factory_;
    wil::com_ptr<IDWriteTextFormat> body_text_format_;
    wil::com_ptr<IDWriteTextFormat> icon_text_format_;
    wil::com_ptr<IRawElementProviderSimple> accessibility_provider_;
    UINT surface_width_ = 0;
    UINT surface_height_ = 0;
};
