#include "imgviewer.about.hpp"

#include <array>
#include <cwchar>
#include <memory>

#include <d2d1helper.h>
#include <wil/result_macros.h>

#include "imgviewer.strings.hpp"
#include "imgviewer.hpp"
#include "imgviewer.action.hpp"
#include "imgviewer.messages.hpp"
#include "imgviewer.ui.action.hpp"
#include "win32.util.hpp"
#include "ui.draw.hpp"
#include "ui.theme.hpp"
#include "ui.window.hpp"

namespace {

constexpr wchar_t kAboutClassName[] = L"ImgViewerAboutWindow";
constexpr int kAboutInitialWidth = 280;
constexpr int kAboutInitialHeight = 260;
constexpr int kAboutMinClientWidth = 240;
constexpr int kAboutMinClientHeight = 210;
constexpr float kAboutSidePadding = 14.0f;

constexpr float kAboutHeaderTop = 12.0f;
constexpr float kAboutHeaderHeight = 17.0f;
constexpr float kAboutSubtitleRow1Top = 33.0f;
constexpr float kAboutSubtitleHeight = 14.0f;
constexpr float kAboutSubtitleRow2Top = 51.0f;
constexpr float kAboutSectionTop = 79.0f;
constexpr float kAboutSectionHeight = 14.0f;
constexpr float kAboutBorderTop = 97.0f;
constexpr float kAboutFooterHeight = 10.0f;
constexpr float kAboutNoticeStartY = 105.0f;
constexpr float kAboutNoticeIndent = 22.0f;
constexpr float kAboutNoticeLineHeight = 12.0f;
constexpr float kAboutNoticeNameGap = 13.0f;
constexpr float kAboutNoticeDetailGap = 12.0f;
constexpr float kAboutNoticeEntryGap = 20.0f;

struct NoticeLine final {
    const wchar_t* name;
    const wchar_t* detail;
    const wchar_t* license_path;
};

constexpr std::array<NoticeLine, 3> kNoticeLines{{
    {L"stb_image.h v2.30", L"Sean Barrett - MIT License or Public Domain", L"third_parties/stb/LICENSE"},
    {L"nlohmann_json", L"Niels Lohmann - MIT License", L"third_parties/nlohmann_json/LICENSE.MIT"},
    {L"Windows Implementation Libraries", L"Microsoft Corporation - MIT License", L"third_parties/wil/LICENSE"},
}};

class AboutUi final : public UiRoot {
public:
    AboutUi()
    {
        root_ = std::make_unique<UiElement>(
            UiRootMetadata(
                UiElementRole::Pane,
                UiActionFromImgViewerAction(ImgViewerAction::None),
                ImgViewerString(ImgViewerStringId::AboutImgViewer),
                ImgViewerString(ImgViewerStringId::AboutImgViewer),
                L"about-root"));
    }

    UiElement* Root() override { return root_.get(); }
    const UiElement* Root() const override { return root_.get(); }
    const wchar_t* AccessibilityRootName() const override { return ImgViewerString(ImgViewerStringId::AboutImgViewer); }

    D2D1_SIZE_F Measure(const UiDrawContext&, D2D1_SIZE_F available_size) override { return available_size; }

    void Arrange(D2D1_RECT_F final_rect) override
    {
        root_->Arrange(final_rect);
    }

    void Render(const UiDrawContext& context, UiRootState) override
    {
        const D2D1_SIZE_F size = context.viewport_size;

        const UiDraw draw(context);
        draw.Clear(ui_theme::color::kWindowBackground);
        draw.DrawBodyText(ImgViewerString(ImgViewerStringId::AppName), static_cast<UINT32>(std::wcslen(ImgViewerString(ImgViewerStringId::AppName))), D2D1::RectF(kAboutSidePadding, kAboutHeaderTop, size.width - kAboutSidePadding, kAboutHeaderTop + kAboutHeaderHeight), ui_theme::color::kBodyText);
        draw.DrawBodyText(
            ImgViewerString(ImgViewerStringId::AboutDescription),
            static_cast<UINT32>(std::wcslen(ImgViewerString(ImgViewerStringId::AboutDescription))),
            D2D1::RectF(kAboutSidePadding, kAboutSubtitleRow1Top, size.width - kAboutSidePadding, kAboutSubtitleRow1Top + kAboutSubtitleHeight),
            ui_theme::color::kMutedText);
        draw.DrawBodyText(
            ImgViewerString(ImgViewerStringId::DevelopmentBuild),
            static_cast<UINT32>(std::wcslen(ImgViewerString(ImgViewerStringId::DevelopmentBuild))),
            D2D1::RectF(kAboutSidePadding, kAboutSubtitleRow2Top, size.width - kAboutSidePadding, kAboutSubtitleRow2Top + kAboutSubtitleHeight),
            ui_theme::color::kMutedText);

        draw.DrawBodyText(
            ImgViewerString(ImgViewerStringId::ThirdPartyNotices),
            static_cast<UINT32>(std::wcslen(ImgViewerString(ImgViewerStringId::ThirdPartyNotices))),
            D2D1::RectF(kAboutSidePadding, kAboutSectionTop, size.width - kAboutSidePadding, kAboutSectionTop + kAboutSectionHeight),
            ui_theme::color::kBodyText);
        draw.DrawRect(D2D1::RectF(kAboutSidePadding, kAboutBorderTop, size.width - kAboutSidePadding, size.height - kAboutFooterHeight), ui_theme::color::kBorder);

        float y = kAboutNoticeStartY;
        for (const NoticeLine& line : kNoticeLines) {
            draw.DrawBodyText(
                line.name,
                static_cast<UINT32>(wcslen(line.name)),
                D2D1::RectF(kAboutNoticeIndent, y, size.width - kAboutNoticeIndent, y + kAboutNoticeLineHeight),
                ui_theme::color::kBodyText,
                D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
            y += kAboutNoticeNameGap;
            draw.DrawBodyText(
                line.detail,
                static_cast<UINT32>(wcslen(line.detail)),
                D2D1::RectF(kAboutNoticeIndent, y, size.width - kAboutNoticeIndent, y + kAboutNoticeLineHeight),
                ui_theme::color::kMutedText,
                D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
            y += kAboutNoticeDetailGap;
            draw.DrawBodyText(
                line.license_path,
                static_cast<UINT32>(wcslen(line.license_path)),
                D2D1::RectF(kAboutNoticeIndent, y, size.width - kAboutNoticeIndent, y + kAboutNoticeLineHeight),
                ui_theme::color::kMutedText,
                D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
            y += kAboutNoticeEntryGap;
        }

    }

    UiEventResult OnKeyEvent(const UiKeyEvent& event) override
    {
        if (event.type == UiEventType::KeyDown && event.virtual_key == VK_ESCAPE) {
            return UiEventResult{.handled = true, .action = ImgViewerAction::CloseAbout};
        }
        return {};
    }

private:
    std::unique_ptr<UiElement> root_;
};

struct AboutWindowContext final : public UiWindowDelegate {
    HWND owner = nullptr;
    UiWindowHost host;

    HRESULT OnCreate(UiWindowHost&) override
    {
        return S_OK;
    }

    void OnDestroy(UiWindowHost&) override
    {
        if (owner != nullptr) {
            PostMessageW(owner, kImgViewerAboutDestroyedMessage, 0, reinterpret_cast<LPARAM>(this));
        }
    }

    bool OnUiAction(UiWindowHost& window_host, UiAction action) override
    {
        switch (ImgViewerActionFromUiAction(action)) {
        case ImgViewerAction::CloseAbout:
            window_host.Close();
            return true;
        default:
            break;
        }
        return false;
    }

    win32::WindowMessageResult OnUnhandledMessage(
        UiWindowHost& window_host,
        UINT message,
        WPARAM,
        LPARAM lparam) override
    {
        if (message == WM_GETMINMAXINFO) {
            util::ApplyMinTrackSize(window_host.Hwnd(), lparam, kAboutMinClientWidth, kAboutMinClientHeight);
            return win32::WindowMessageResult::Handled();
        }
        return win32::WindowMessageResult::Unhandled();
    }
};

} // namespace

HRESULT OpenImgViewerAboutWindow(HWND owner, ImgViewerContext* context)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, owner);
    RETURN_HR_IF_NULL(E_INVALIDARG, context);

    if (context->about_context != nullptr) {
        auto* about_context = static_cast<AboutWindowContext*>(context->about_context);
        if (about_context->host.Hwnd() != nullptr && IsWindow(about_context->host.Hwnd())) {
            ShowWindow(about_context->host.Hwnd(), SW_SHOWNORMAL);
            SetForegroundWindow(about_context->host.Hwnd());
            return S_OK;
        }
    }

    HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner, GWLP_HINSTANCE));
    RETURN_HR_IF_NULL(E_UNEXPECTED, instance);

    auto* about_context = new (std::nothrow) AboutWindowContext();
    RETURN_IF_NULL_ALLOC(about_context);
    about_context->owner = owner;
    context->about_context = about_context;

    auto root = std::make_unique<AboutUi>();
    const HRESULT create_hr = about_context->host.Create(
        UiWindowOptions{
            .native = win32::NativeWindowOptions{
                .instance = instance,
                .class_name = kAboutClassName,
                .title = ImgViewerString(ImgViewerStringId::AboutImgViewer),
                .style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
                .ex_style = WS_EX_DLGMODALFRAME,
                .width = kAboutInitialWidth,
                .height = kAboutInitialHeight,
                .owner = owner,
                .show_command = SW_SHOWNORMAL,
            },
            .action_message = kImgViewerUiActionMessage,
            .body_font_size = 9.0f,
            .icon_font_size = 11.0f,
        },
        std::move(root),
        about_context,
        &context->graphics_device);
    if (FAILED(create_hr)) {
        context->about_context = nullptr;
        delete about_context;
        RETURN_IF_FAILED(create_hr);
    }

    about_context->host.Window().Show(SW_SHOWNORMAL);
    context->interaction.SetModal(ImgViewerModalOwner::About);
    return S_OK;
}

void CleanupImgViewerAboutWindow(ImgViewerContext* context, void* about_context)
{
    if (context != nullptr && context->about_context == about_context) {
        context->about_context = nullptr;
        context->interaction.ClearModal(ImgViewerModalOwner::About);
    }
    delete static_cast<AboutWindowContext*>(about_context);
}
