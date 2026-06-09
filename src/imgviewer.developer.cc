#include "imgviewer.developer.hpp"

#include <cstdio>
#include <memory>
#include <optional>
#include <string>

#include <d2d1helper.h>
#include <wil/result_macros.h>

#include "imgviewer.hpp"
#include "imgviewer.action.hpp"
#include "imgviewer.messages.hpp"
#include "imgviewer.ui.action.hpp"
#include "ui.button.hpp"
#include "ui.draw.hpp"
#include "ui.label.hpp"
#include "ui.panel.hpp"
#include "ui.selection.hpp"
#include "ui.slider.hpp"
#include "ui.table.hpp"
#include "ui.theme.hpp"
#include "ui.window.hpp"
#include "win32.util.hpp"

namespace {

constexpr wchar_t kDeveloperClassName[] = L"ImgViewerDeveloperWindow";
constexpr wchar_t kCloseIcon[] = L"\xE711";
constexpr wchar_t kRefreshIcon[] = L"\xE72C";
constexpr int kDeveloperInitialWidth = 460;
constexpr int kDeveloperInitialHeight = 360;
constexpr int kDeveloperMinClientWidth = 360;
constexpr int kDeveloperMinClientHeight = 260;
constexpr float kDeveloperSidePadding = 14.0f;
constexpr float kDeveloperContentTopPadding = 12.0f;
constexpr float kDeveloperFooterBottomPadding = 10.0f;
constexpr float kDeveloperFooterButtonHeight = 24.0f;
constexpr float kDeveloperFooterButtonGap = 6.0f;

class DeveloperUi final : public UiRoot {
public:
    DeveloperUi()
    {
        auto root = std::make_unique<StackPanel>(
            UiRootMetadata(UiElementRole::Pane, kUiActionNone, L"Developer", L"Developer", L"developer-root"));
        root->SetPadding(UiThickness{kDeveloperSidePadding, kDeveloperContentTopPadding, kDeveloperSidePadding, 0.0f});
        root->SetGap(8.0f);
        root_ = root.get();
        root_owner_ = std::move(root);

        root_->AddItem(std::make_unique<Label>(
            UiMetadata(UiElementRole::Text, kUiActionNone, L"Developer", L"", L"developer-title", false, true),
            L"Developer",
            LabelStyle::Title), 20.0f);
        root_->AddItem(std::make_unique<Label>(
            UiMetadata(UiElementRole::Text, kUiActionNone, L"Control Lab", L"", L"developer-subtitle", false, true),
            L"Control Lab",
            LabelStyle::Muted), 18.0f);

        sample_button_ = root_->AddItem(std::make_unique<Button>(
            UiMetadata(UiElementRole::Button, UiActionFromImgViewerAction(ImgViewerAction::DeveloperSampleButton),
                L"Sample Button", L"Sample Button", L"developer-sample-button"),
            kRefreshIcon,
            L"Sample Button"), 28.0f);
        sample_checkbox_ = root_->AddItem(std::make_unique<Checkbox>(
            UiMetadata(UiElementRole::CheckBox, kUiActionNone, L"Sample checkbox", L"Sample checkbox", L"developer-sample-checkbox"),
            L"Sample checkbox",
            sample_checked_), 24.0f);
        sample_slider_ = root_->AddItem(std::make_unique<SliderRow>(
            UiMetadata(UiElementRole::Slider, kUiActionNone, L"Sample slider", L"Sample slider", L"developer-sample-slider"),
            0,
            100,
            sample_slider_value_,
            1,
            10), 28.0f);
        editable_table_ = root_->AddItem(std::make_unique<Table>(
            UiMetadata(UiElementRole::Pane, kUiActionNone, L"Editable table", L"Editable table", L"developer-editable-table")),
            112.0f);
        editable_table_->SetColumns(std::vector<TableColumn>{
            TableColumn{L"Name", 120.0f, false},
            TableColumn{L"Value", 0.0f, true, true},
            TableColumn{
                .header = L"Kind",
                .width = 110.0f,
                .editable = true,
                .editor = TableCellEditor::Dropdown,
                .dropdown_options = {L"Shortcut", L"Label", L"Number", L"Token"},
            },
        });
        editable_table_->SetHeaderVisible(true);
        editable_table_->SetSelectionEnabled(true);
        editable_table_->SetRowHeight(22.0f);
        editable_table_->SetRows(std::vector<TableRow>{
            TableRow{.cells = {L"Sample shortcut", L"Ctrl+1", L"Shortcut"}},
            TableRow{.cells = {L"Visible label", L"Developer", L"Label"}},
            TableRow{.cells = {L"Numeric value", L"50", L"Number"}},
            TableRow{.cells = {L"Theme token", L"Accent", L"Token"}},
        });
        state_label_ = root_->AddItem(std::make_unique<Label>(
            UiMetadata(UiElementRole::Text, kUiActionNone, L"Developer state", L"", L"developer-state", false, true),
            L"",
            LabelStyle::Body), 22.0f);

        close_button_ = root_->AddItem(std::make_unique<Button>(
            UiMetadata(UiElementRole::Button, UiActionFromImgViewerAction(ImgViewerAction::CloseDeveloper),
                L"Close", L"Close", L"close-developer"),
            kCloseIcon,
            L"Close"), 28.0f);
        UpdateStateText();
    }

    UiElement* Root() override { return root_owner_.get(); }
    const UiElement* Root() const override { return root_owner_.get(); }
    const wchar_t* AccessibilityRootName() const override { return L"Developer"; }

    UiEventResult OnInputEvent(const UiInputEvent& event) override
    {
        if (editable_table_ != nullptr && editable_table_->IsEditing() && event.focused == editable_table_->EditorId()) {
            return editable_table_->OnInputEvent(event);
        }
        return {};
    }

    UiEventResult OnPointerEvent(const UiPointerEvent& event) override
    {
        if (event.type != UiEventType::PointerDown ||
            editable_table_ == nullptr ||
            !editable_table_->IsEditing() ||
            editable_table_->Contains(event.point)) {
            return {};
        }

        UiEventResult result = editable_table_->CommitEdit();
        if (result.value_changed) {
            ApplyTableEditCommit();
        }
        result.handled = false;
        return result;
    }

    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) override
    {
        sample_button_width_ = sample_button_->PreferredWidth(context);
        close_button_width_ = close_button_->PreferredWidth(context);
        return root_->Measure(context, available_size);
    }

    void Arrange(D2D1_RECT_F final_rect) override
    {
        root_owner_->Arrange(final_rect);
        sample_button_->Arrange(D2D1::RectF(
            final_rect.left + kDeveloperSidePadding,
            sample_button_->Rect().top,
            final_rect.left + kDeveloperSidePadding + sample_button_width_,
            sample_button_->Rect().bottom));

        close_button_->Arrange(D2D1::RectF(
            final_rect.right - kDeveloperSidePadding - close_button_width_,
            final_rect.bottom - kDeveloperFooterBottomPadding - kDeveloperFooterButtonHeight,
            final_rect.right - kDeveloperSidePadding,
            final_rect.bottom - kDeveloperFooterBottomPadding));
    }

    void Render(const UiDrawContext& context, UiRootState state) override
    {
        const UiDraw draw(context);
        draw.Clear(ui_theme::color::kWindowBackground);
        root_owner_->Render(context, state);
    }

    UiEventResult OnKeyEvent(const UiKeyEvent& event) override
    {
        if (editable_table_ != nullptr && editable_table_->IsEditing() && event.focused == editable_table_->EditorId()) {
            UiEventResult result = editable_table_->OnKeyEvent(event);
            if (result.value_changed) {
                ApplyTableEditCommit();
            }
            return result;
        }
        if (event.type == UiEventType::KeyDown && event.virtual_key == VK_ESCAPE) {
            return UiEventResult{.handled = true, .action = ImgViewerAction::CloseDeveloper};
        }
        return {};
    }

    UiEventResult ExecuteTextAction(UiAction action, HWND hwnd)
    {
        if (editable_table_ == nullptr || !editable_table_->IsEditing()) {
            return {};
        }
        return editable_table_->ExecuteEditAction(action, hwnd);
    }

    void ApplyElementEffect(UiElementId id) override
    {
        if (id == sample_button_->Id()) {
            ++sample_button_clicks_;
            UpdateStateText();
            return;
        }
        if (id == sample_checkbox_->Id()) {
            sample_checked_ = !sample_checked_;
            sample_checkbox_->SetChecked(sample_checked_);
            UpdateStateText();
            return;
        }
        if (id == sample_slider_->GetSlider()->Id()) {
            sample_slider_value_ = sample_slider_->Value();
            UpdateStateText();
            return;
        }
        if (id == editable_table_->Id() || editable_table_->IsEditorElement(id)) {
            if (editable_table_->IsEditorElement(id)) {
                editable_table_->CommitEdit();
            }
            ApplyTableEditCommit();
            return;
        }
    }

    const wchar_t* ElementValue(UiElementId id) const override
    {
        if (id == state_label_->Id()) {
            return state_text_.c_str();
        }
        if (id == sample_slider_->GetSlider()->Id()) {
            return slider_value_text_.c_str();
        }
        if (editable_table_ != nullptr && id == editable_table_->EditorId()) {
            return L"";
        }
        return L"";
    }

    double ElementRangeValue(UiElementId id) const override
    {
        return id == sample_slider_->GetSlider()->Id() ? static_cast<double>(sample_slider_->Value()) : 0.0;
    }

    double ElementRangeMinimum(UiElementId id) const override
    {
        return id == sample_slider_->GetSlider()->Id() ? 0.0 : 0.0;
    }

    double ElementRangeMaximum(UiElementId id) const override
    {
        return id == sample_slider_->GetSlider()->Id() ? 100.0 : 0.0;
    }

    HRESULT SetElementRangeValue(UiElementId id, double value) override
    {
        if (id != sample_slider_->GetSlider()->Id()) {
            return E_NOTIMPL;
        }
        sample_slider_->SetValue(static_cast<int>(value + 0.5));
        sample_slider_value_ = sample_slider_->Value();
        UpdateStateText();
        return S_OK;
    }

private:
    void UpdateStateText()
    {
        wchar_t state[128] = {};
        swprintf_s(
            state,
            L"Clicks: %d   Checkbox: %s   Slider: %d   Edit: %s",
            sample_button_clicks_,
            sample_checked_ ? L"on" : L"off",
            sample_slider_value_,
            last_table_edit_.c_str());
        state_text_ = state;
        slider_value_text_ = std::to_wstring(sample_slider_value_);
        if (state_label_ != nullptr) {
            state_label_->SetText(state_text_.c_str());
        }
        if (sample_slider_ != nullptr) {
            sample_slider_->SetValueText(slider_value_text_.c_str());
        }
    }

    void ApplyTableEditCommit()
    {
        if (editable_table_ == nullptr) {
            return;
        }
        const std::optional<TableEditCommit> commit = editable_table_->TakeEditCommit();
        if (!commit.has_value()) {
            return;
        }
        last_table_edit_ = L"row ";
        last_table_edit_ += std::to_wstring(commit->row + 1);
        last_table_edit_ += L", col ";
        last_table_edit_ += std::to_wstring(commit->column + 1);
        last_table_edit_ += L" = ";
        last_table_edit_ += commit->value;
        UpdateStateText();
    }

    std::unique_ptr<UiElement> root_owner_;
    StackPanel* root_ = nullptr;
    Button* sample_button_ = nullptr;
    Checkbox* sample_checkbox_ = nullptr;
    SliderRow* sample_slider_ = nullptr;
    Table* editable_table_ = nullptr;
    Label* state_label_ = nullptr;
    Button* close_button_ = nullptr;
    std::wstring state_text_;
    std::wstring slider_value_text_;
    std::wstring last_table_edit_ = L"none";
    int sample_button_clicks_ = 0;
    bool sample_checked_ = false;
    int sample_slider_value_ = 50;
    float sample_button_width_ = 0.0f;
    float close_button_width_ = 0.0f;
};

struct DeveloperWindowContext final : public UiWindowDelegate {
    HWND owner = nullptr;
    UiWindowHost host;
    bool standalone = false;
    DeveloperUi* ui = nullptr;

    void OnDestroy(UiWindowHost&) override
    {
        if (owner != nullptr) {
            PostMessageW(owner, kImgViewerDeveloperDestroyedMessage, 0, reinterpret_cast<LPARAM>(this));
        } else if (standalone) {
            PostQuitMessage(0);
        }
    }

    bool OnUiAction(UiWindowHost& window_host, UiAction action) override
    {
        if (action == kUiActionTextCopy ||
            action == kUiActionTextCut ||
            action == kUiActionTextPaste ||
            action == kUiActionTextSelectAll) {
            if (ui != nullptr) {
                const UiEventResult result = ui->ExecuteTextAction(action, window_host.Hwnd());
                if (result.needs_render) {
                    window_host.Invalidate();
                }
            }
            return true;
        }

        if (ImgViewerActionFromUiAction(action) == ImgViewerAction::CloseDeveloper) {
            window_host.Close();
            return true;
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
            util::ApplyMinTrackSize(window_host.Hwnd(), lparam, kDeveloperMinClientWidth, kDeveloperMinClientHeight);
            return win32::WindowMessageResult::Handled();
        }
        return win32::WindowMessageResult::Unhandled();
    }
};

} // namespace

HRESULT OpenImgViewerDeveloperWindow(HWND owner, ImgViewerContext* context)
{
#if defined(IMGVIEWER_ENABLE_DEVELOPER_WINDOW)
    RETURN_HR_IF_NULL(E_INVALIDARG, context);

    if (context->developer_context != nullptr) {
        auto* developer_context = static_cast<DeveloperWindowContext*>(context->developer_context);
        if (developer_context->host.Hwnd() != nullptr && IsWindow(developer_context->host.Hwnd())) {
            ShowWindow(developer_context->host.Hwnd(), SW_SHOWNORMAL);
            SetForegroundWindow(developer_context->host.Hwnd());
            return S_OK;
        }
    }

    HINSTANCE instance = owner != nullptr
        ? reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner, GWLP_HINSTANCE))
        : GetModuleHandleW(nullptr);
    RETURN_HR_IF_NULL(E_UNEXPECTED, instance);

    auto* developer_context = new (std::nothrow) DeveloperWindowContext();
    RETURN_IF_NULL_ALLOC(developer_context);
    developer_context->owner = owner;
    context->developer_context = developer_context;

    auto root = std::make_unique<DeveloperUi>();
    developer_context->ui = root.get();
    const HRESULT create_hr = developer_context->host.Create(
        UiWindowOptions{
            .native = win32::NativeWindowOptions{
                .instance = instance,
                .class_name = kDeveloperClassName,
                .title = L"Developer",
                .style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
                .ex_style = WS_EX_DLGMODALFRAME,
                .width = kDeveloperInitialWidth,
                .height = kDeveloperInitialHeight,
                .owner = owner,
                .show_command = SW_SHOWNORMAL,
            },
            .action_message = kImgViewerUiActionMessage,
            .body_font_size = 9.0f,
            .icon_font_size = 11.0f,
        },
        std::move(root),
        developer_context);
    if (FAILED(create_hr)) {
        context->developer_context = nullptr;
        delete developer_context;
        RETURN_IF_FAILED(create_hr);
    }

    developer_context->host.Window().Show(SW_SHOWNORMAL);
    context->interaction.SetModal(ImgViewerModalOwner::Developer);
    return S_OK;
#else
    UNREFERENCED_PARAMETER(owner);
    UNREFERENCED_PARAMETER(context);
    return S_FALSE;
#endif
}

void CleanupImgViewerDeveloperWindow(ImgViewerContext* context, void* developer_context)
{
#if defined(IMGVIEWER_ENABLE_DEVELOPER_WINDOW)
    if (context != nullptr && context->developer_context == developer_context) {
        context->developer_context = nullptr;
        context->interaction.ClearModal(ImgViewerModalOwner::Developer);
    }
    delete static_cast<DeveloperWindowContext*>(developer_context);
#else
    UNREFERENCED_PARAMETER(context);
    UNREFERENCED_PARAMETER(developer_context);
#endif
}

HRESULT RunImgViewerDeveloperWindowApplication()
{
#if defined(IMGVIEWER_ENABLE_DEVELOPER_WINDOW)
    RETURN_IF_FAILED(util::InitializeDpiAwareness());
    const HRESULT co_initialize_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    RETURN_IF_FAILED(co_initialize_result);
    auto co_uninitialize = wil::scope_exit([] { CoUninitialize(); });

    ImgViewerContext context;
    RETURN_IF_FAILED(LoadImgViewerConfig(&context.config));

    RETURN_IF_FAILED(OpenImgViewerDeveloperWindow(nullptr, &context));
    auto* developer_context = static_cast<DeveloperWindowContext*>(context.developer_context);
    if (developer_context != nullptr) {
        developer_context->standalone = true;
    }

    MSG message = {};
    while (true) {
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result == -1) {
            RETURN_LAST_ERROR();
        }
        if (result == 0) {
            break;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return S_OK;
#else
    return E_NOTIMPL;
#endif
}
