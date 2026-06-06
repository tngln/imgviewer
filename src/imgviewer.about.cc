#include "imgviewer.about.hpp"

#include <array>
#include <cwchar>
#include <memory>
#include <string>
#include <vector>

#include <d2d1helper.h>
#include <wil/result_macros.h>

#include "imgviewer.hpp"
#include "imgviewer.action.hpp"
#include "imgviewer.messages.hpp"
#include "imgviewer.ui.action.hpp"
#include "ui.button.hpp"
#include "ui.draw.hpp"
#include "ui.layout.hpp"
#include "ui.theme.hpp"
#include "ui.window.hpp"
#include "win32.clipboard.hpp"

namespace {

constexpr wchar_t kAboutClassName[] = L"ImgViewerAboutWindow";
constexpr wchar_t kCloseIcon[] = L"\xE711";
constexpr wchar_t kCopyIcon[] = L"\xE8C8";
constexpr int kAboutInitialWidth = 560;
constexpr int kAboutInitialHeight = 520;
constexpr int kAboutMinClientWidth = 480;
constexpr int kAboutMinClientHeight = 420;
constexpr float kAboutSidePadding = 28.0f;
constexpr float kAboutFooterBottomPadding = 20.0f;
constexpr float kAboutFooterButtonHeight = 48.0f;

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

std::wstring AboutNoticesText()
{
    std::wstring text =
        L"ImgViewer\n"
        L"Development build\n"
        L"Lightweight native image viewer.\n\n"
        L"Third-party notices:\n";

    for (const NoticeLine& line : kNoticeLines) {
        text += L"\n";
        text += line.name;
        text += L"\n";
        text += line.detail;
        text += L"\n";
        text += line.license_path;
        text += L"\n";
    }
    return text;
}

class AboutUi final : public UiRoot {
public:
    AboutUi()
    {
        root_ = std::make_unique<UiElement>(
            UiRootMetadata(
                UiElementRole::Pane,
                UiActionFromImgViewerAction(ImgViewerAction::None),
                L"About ImgViewer",
                L"About ImgViewer",
                L"about-root"));
        copy_button_ = static_cast<Button*>(root_->AddChild(std::make_unique<Button>(
            UiMetadata(
                UiElementRole::Button,
                UiActionFromImgViewerAction(ImgViewerAction::CopyAboutNotices),
                L"Copy Notices",
                L"Copy Notices",
                L"copy-about-notices"),
            kCopyIcon,
            L"Copy Notices")));
        close_button_ = static_cast<Button*>(root_->AddChild(std::make_unique<Button>(
            UiMetadata(
                UiElementRole::Button,
                UiActionFromImgViewerAction(ImgViewerAction::CloseAbout),
                L"Close",
                L"Close",
                L"close-about"),
            kCloseIcon,
            L"Close")));
    }

    UiElement* Root() override { return root_.get(); }
    const UiElement* Root() const override { return root_.get(); }
    const wchar_t* AccessibilityRootName() const override { return L"About ImgViewer"; }

    void Draw(const UiDrawContext& context, UiRootState state) override
    {
        const D2D1_SIZE_F size = context.viewport_size;
        Layout(context);

        const UiDraw draw(context);
        draw.Clear(ui_theme::color::kWindowBackground);
        draw.DrawBodyText(L"ImgViewer", 9, D2D1::RectF(28.0f, 24.0f, size.width - 28.0f, 58.0f), ui_theme::color::kBodyText);
        draw.DrawBodyText(
            L"Lightweight native image viewer.",
            32,
            D2D1::RectF(28.0f, 66.0f, size.width - 28.0f, 94.0f),
            ui_theme::color::kMutedText);
        draw.DrawBodyText(
            L"Development build",
            17,
            D2D1::RectF(28.0f, 102.0f, size.width - 28.0f, 130.0f),
            ui_theme::color::kMutedText);

        draw.DrawBodyText(
            L"Third-party notices",
            19,
            D2D1::RectF(28.0f, 158.0f, size.width - 28.0f, 186.0f),
            ui_theme::color::kBodyText);
        draw.DrawRect(D2D1::RectF(28.0f, 194.0f, size.width - 28.0f, size.height - 92.0f), ui_theme::color::kBorder);

        float y = 210.0f;
        for (const NoticeLine& line : kNoticeLines) {
            draw.DrawBodyText(
                line.name,
                static_cast<UINT32>(wcslen(line.name)),
                D2D1::RectF(44.0f, y, size.width - 44.0f, y + 24.0f),
                ui_theme::color::kBodyText,
                D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
            y += 26.0f;
            draw.DrawBodyText(
                line.detail,
                static_cast<UINT32>(wcslen(line.detail)),
                D2D1::RectF(44.0f, y, size.width - 44.0f, y + 24.0f),
                ui_theme::color::kMutedText,
                D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
            y += 24.0f;
            draw.DrawBodyText(
                line.license_path,
                static_cast<UINT32>(wcslen(line.license_path)),
                D2D1::RectF(44.0f, y, size.width - 44.0f, y + 24.0f),
                ui_theme::color::kMutedText,
                D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
            y += 40.0f;
        }

        DrawElement(*copy_button_, context, state);
        DrawElement(*close_button_, context, state);
    }

    UiEventResult OnKeyEvent(const UiKeyEvent& event) override
    {
        if (event.type == UiEventType::KeyDown && event.virtual_key == VK_ESCAPE) {
            return UiEventResult{.handled = true, .action = ImgViewerAction::CloseAbout};
        }
        return {};
    }

private:
    void Layout(const UiDrawContext& context)
    {
        const D2D1_SIZE_F size = context.viewport_size;
        root_->SetRect(D2D1::RectF(0.0f, 0.0f, size.width, size.height));
        copy_button_->SetRect(D2D1::RectF(
            kAboutSidePadding,
            size.height - kAboutFooterBottomPadding - kAboutFooterButtonHeight,
            kAboutSidePadding + copy_button_->PreferredWidth(context),
            size.height - kAboutFooterBottomPadding));
        const std::vector<D2D1_RECT_F> primary_buttons = ui_layout::PlaceBottomRightRow(
            root_->Rect(),
            std::vector<float>{close_button_->PreferredWidth(context)},
            kAboutFooterButtonHeight,
            kAboutFooterBottomPadding,
            kAboutFooterBottomPadding);
        close_button_->SetRect(primary_buttons[0]);
    }

    void DrawElement(UiElement& element, const UiDrawContext& context, UiRootState state) const
    {
        element.Draw(
            context,
            UiElementState{
                .hovered = state.hovered == element.Id(),
                .pressed = state.pressed == element.Id(),
                .active = state.focused == element.Id(),
                .enabled = element.IsEnabled(),
            });
    }

    std::unique_ptr<UiElement> root_;
    Button* copy_button_ = nullptr;
    Button* close_button_ = nullptr;
};

struct AboutWindowContext final : public UiWindowDelegate {
    HWND owner = nullptr;
    ImgViewerContext* app = nullptr;
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
        case ImgViewerAction::CopyAboutNotices:
            if (win32::CopyTextToClipboard(window_host.Hwnd(), AboutNoticesText().c_str())) {
                ShowImgViewerToast(owner, app, L"Copied notices.");
            } else {
                ShowImgViewerToast(owner, app, L"Could not copy notices.");
            }
            return true;
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
            auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
            if (info != nullptr) {
                RECT min_rect{0, 0, kAboutMinClientWidth, kAboutMinClientHeight};
                const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(window_host.Hwnd(), GWL_STYLE));
                const DWORD ex_style = static_cast<DWORD>(GetWindowLongPtrW(window_host.Hwnd(), GWL_EXSTYLE));
                AdjustWindowRectEx(&min_rect, style, FALSE, ex_style);
                info->ptMinTrackSize.x = min_rect.right - min_rect.left;
                info->ptMinTrackSize.y = min_rect.bottom - min_rect.top;
            }
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
    about_context->app = context;
    context->about_context = about_context;

    auto root = std::make_unique<AboutUi>();
    const HRESULT create_hr = about_context->host.Create(
        UiWindowOptions{
            .native = win32::NativeWindowOptions{
                .instance = instance,
                .class_name = kAboutClassName,
                .title = L"About ImgViewer",
                .style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
                .ex_style = WS_EX_DLGMODALFRAME,
                .width = kAboutInitialWidth,
                .height = kAboutInitialHeight,
                .owner = owner,
                .show_command = SW_SHOWNORMAL,
            },
            .action_message = kImgViewerUiActionMessage,
            .body_font_size = 18.0f,
            .icon_font_size = 22.0f,
        },
        std::move(root),
        about_context);
    if (FAILED(create_hr)) {
        context->about_context = nullptr;
        delete about_context;
        RETURN_IF_FAILED(create_hr);
    }

    about_context->host.Window().Show(SW_SHOWNORMAL);
    return S_OK;
}

void CleanupImgViewerAboutWindow(ImgViewerContext* context, void* about_context)
{
    if (context != nullptr && context->about_context == about_context) {
        context->about_context = nullptr;
    }
    delete static_cast<AboutWindowContext*>(about_context);
}
