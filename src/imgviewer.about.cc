#include "imgviewer.about.hpp"

#include <array>
#include <memory>

#include <d2d1helper.h>
#include <wil/result_macros.h>

#include "imgviewer.strings.hpp"
#include "imgviewer.hpp"
#include "imgviewer.action.hpp"
#include "imgviewer.messages.hpp"
#include "imgviewer.ui.action.hpp"
#include "ui.label.hpp"
#include "ui.panel.hpp"
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
constexpr float kAboutTopPadding = 12.0f;
constexpr float kAboutSectionGap = 10.0f;
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

UiElementMetadata PaneMetadata(const wchar_t* automation_id)
{
    return UiMetadata(UiElementRole::Pane, kUiActionNone, L"", L"", automation_id, false, false);
}

UiElementMetadata LabelMetadata(const wchar_t* label, const wchar_t* automation_id)
{
    return UiMetadata(UiElementRole::Text, kUiActionNone, label, L"", automation_id, false, false);
}

class BorderedPanel final : public UiElement {
public:
    explicit BorderedPanel(UiElementMetadata metadata) : UiElement(metadata)
    {
        panel_ = static_cast<StackPanel*>(AddChild(std::make_unique<StackPanel>(PaneMetadata(L"about-notices-box-content"))));
    }

    void SetGap(float gap)
    {
        panel_->SetGap(gap);
    }

    void SetPadding(UiThickness padding)
    {
        panel_->SetPadding(padding);
    }

    template <typename T>
    T* AddItem(std::unique_ptr<T> child, float fixed_main_size = 0.0f)
    {
        return panel_->AddItem(std::move(child), fixed_main_size);
    }

    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const override
    {
        return panel_->Measure(context, available_size);
    }

    void Arrange(D2D1_RECT_F final_rect) override
    {
        UiElement::Arrange(final_rect);
        panel_->Arrange(final_rect);
    }

    void Render(const UiDrawContext& context, UiRootState state) const override
    {
        const UiDraw draw(context);
        draw.DrawRect(Rect(), ui_theme::color::kBorder);
        panel_->Render(context, state);
    }

private:
    StackPanel* panel_ = nullptr;
};

class AboutUi final : public UiRoot {
public:
    AboutUi()
    {
        auto root_panel = std::make_unique<StackPanel>(
            UiRootMetadata(
                UiElementRole::Pane,
                UiActionFromImgViewerAction(ImgViewerAction::None),
                ImgViewerString(ImgViewerStringId::AboutImgViewer),
                ImgViewerString(ImgViewerStringId::AboutImgViewer),
                L"about-root"));
        root_panel->SetPadding(UiThickness{kAboutSidePadding, kAboutTopPadding, kAboutSidePadding, 0.0f});
        root_panel->SetGap(0.0f);
        root_ = root_panel.get();
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
    void BuildUiTree()
    {
        root_->AddItem(std::make_unique<Label>(
            LabelMetadata(ImgViewerString(ImgViewerStringId::AppName), L"about-app-name"),
            ImgViewerString(ImgViewerStringId::AppName),
            LabelStyle::Title));
        root_->AddItem(std::make_unique<Label>(
            LabelMetadata(ImgViewerString(ImgViewerStringId::AboutDescription), L"about-description"),
            ImgViewerString(ImgViewerStringId::AboutDescription),
            LabelStyle::Muted));
        root_->AddItem(std::make_unique<Label>(
            LabelMetadata(ImgViewerString(ImgViewerStringId::DevelopmentBuild), L"about-development-build"),
            ImgViewerString(ImgViewerStringId::DevelopmentBuild),
            LabelStyle::Muted));

        auto notices_section = std::make_unique<StackPanel>(PaneMetadata(L"about-notices-section"));
        notices_section->SetPadding(UiThickness{0.0f, kAboutSectionGap, 0.0f, 0.0f});
        notices_section->SetGap(kAboutSectionGap);
        notices_section->AddItem(std::make_unique<Label>(
            LabelMetadata(ImgViewerString(ImgViewerStringId::ThirdPartyNotices), L"about-notices-title"),
            ImgViewerString(ImgViewerStringId::ThirdPartyNotices)));

        auto notice_box = std::make_unique<BorderedPanel>(PaneMetadata(L"about-notices-box"));
        notice_box->SetPadding(UiThickness{
            kAboutNoticeInnerPadding, kAboutNoticeInnerPadding, kAboutNoticeInnerPadding, kAboutNoticeInnerPadding});
        notice_box->SetGap(kAboutNoticeEntryGap);
        for (const NoticeLine& line : kNoticeLines) {
            auto entry = std::make_unique<StackPanel>(PaneMetadata(L"about-notice-entry"));
            entry->SetGap(0.0f);
            entry->AddItem(std::make_unique<Label>(LabelMetadata(line.name, L"about-notice-name"), line.name));
            entry->AddItem(std::make_unique<Label>(LabelMetadata(line.detail, L"about-notice-detail"), line.detail, LabelStyle::Muted));
            entry->AddItem(std::make_unique<Label>(LabelMetadata(line.license_path, L"about-notice-license"), line.license_path, LabelStyle::Muted));
            notice_box->AddItem(std::move(entry));
        }

        notices_section->AddItem(std::move(notice_box));
        root_->AddItem(std::move(notices_section));
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
