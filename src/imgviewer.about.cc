#include "imgviewer.about.hpp"

#include <array>
#include <memory>

#include <wil/result_macros.h>

#include "experimental/ui.decl.hpp"
#include "imgviewer.strings.hpp"
#include "imgviewer.hpp"
#include "imgviewer.action.hpp"
#include "imgviewer.messages.hpp"
#include "imgviewer.ui.action.hpp"
#include "ui.panel.hpp"
#include "win32.util.hpp"
#include "ui.draw.hpp"
#include "ui.theme.hpp"
#include "ui.window.hpp"

namespace {
namespace ui_decl = experimental::ui_decl;

constexpr int kAboutInitialWidth = 560;
constexpr int kAboutInitialHeight = 520;
constexpr int kAboutMinClientWidth = 240;
constexpr int kAboutMinClientHeight = 210;
constexpr float kAboutSidePadding = 14.0f;
constexpr float kAboutTopPadding = 12.0f;
constexpr float kAboutNoticeInnerPadding = 8.0f;
constexpr float kAboutNoticeEntryGap = 7.0f;

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
        auto root_panel = ui_decl::VStack(
            UiRootMetadata(UiElementRole::Pane, ImgViewerString(ImgViewerStringId::AboutImgViewer), kUiTooltipFromName))
            .Padding(UiThickness{kAboutSidePadding, kAboutTopPadding, kAboutSidePadding, 0.0f})
            .Gap(0.0f);
        root_ = root_panel.Get();
        root_owner_ = std::move(root_panel);

        BuildUiTree();
    }

    UiElement* Root() override { return root_owner_.get(); }
    const UiElement* Root() const override { return root_owner_.get(); }
    const wchar_t* AccessibilityRootName() const override { return ImgViewerString(ImgViewerStringId::AboutImgViewer); }

    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) override
    {
        root_owner_->Measure(context, available_size);
        return available_size;
    }

    void Arrange(D2D1_RECT_F final_rect) override
    {
        root_owner_->Arrange(final_rect);
    }

    void Render(const UiDrawContext& context, UiRootState state) override
    {
        const UiDraw draw(context);
        draw.Clear(ui_theme::color::kWindowBackground);
        root_owner_->Render(context, state);
    }

    UiEventResult OnKeyEvent(const UiKeyEvent& event) override
    {
        if (event.type == UiEventType::KeyDown && event.virtual_key == VK_ESCAPE) {
            return UiEventResult{.handled = true, .action = ImgViewerAction::CloseAbout};
        }
        return {};
    }

private:
    static std::unique_ptr<UiElement> BuildNoticeEntry(const NoticeLine& line)
    {
        using namespace experimental::ui_decl;

        return Group(
            Body(line.name),
            Muted(line.detail),
            Muted(line.license_path));
    }

    void BuildUiTree()
    {
        using namespace experimental::ui_decl;

        root_->AddItem(VStack(
            Title(ImgViewerString(ImgViewerStringId::AppName)),
            Muted(ImgViewerString(ImgViewerStringId::AboutDescription)),
            Muted(ImgViewerString(ImgViewerStringId::DevelopmentBuild)),
            Section(
                ImgViewerString(ImgViewerStringId::ThirdPartyNotices),
                BorderBox(
                    BuildNoticeEntry(kNoticeLines[0]),
                    BuildNoticeEntry(kNoticeLines[1]),
                    BuildNoticeEntry(kNoticeLines[2]))
                    .Gap(kAboutNoticeEntryGap)
                    .Padding(UiThickness{
                        kAboutNoticeInnerPadding, kAboutNoticeInnerPadding, kAboutNoticeInnerPadding, kAboutNoticeInnerPadding}))));
    }

    std::unique_ptr<UiElement> root_owner_;
    StackPanel* root_ = nullptr;
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
            PostMessageW(owner, kImgViewerOwnedWindowDestroyedMessage, 0, reinterpret_cast<LPARAM>(static_cast<UiWindowDelegate*>(this)));
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
                .title = ImgViewerString(ImgViewerStringId::AboutImgViewer),
                .frame = win32::NativeWindowFrame::Dialog,
                .width = kAboutInitialWidth,
                .height = kAboutInitialHeight,
                .owner = owner,
            },
            .action_message = kImgViewerUiActionMessage,
            .body_font_size = 9.0f,
            .icon_font_size = 11.0f,
            .script_engine = context->script_engine.get(),
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
