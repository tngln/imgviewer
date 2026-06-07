#include "imgviewer.ui.info_panel.hpp"

#include <algorithm>
#include <array>
#include <cwchar>
#include <memory>
#include <utility>

#include <d2d1helper.h>

#include "math.hpp"
#include "ui.events.hpp"
#include "ui.theme.hpp"

namespace {

constexpr float kPanelWidth = 320.0f;
constexpr float kPanelMargin = 6.0f;
constexpr float kPanelTopPadding = 12.0f;
constexpr float kPanelSidePadding = 12.0f;
constexpr float kHeaderHeight = 22.0f;
constexpr float kRowHeight = 21.0f;
constexpr float kLabelWidth = 112.0f;
constexpr float kHistogramTopGap = 8.0f;
constexpr float kHistogramHeaderHeight = 20.0f;
constexpr float kHistogramTabHeight = 21.0f;
constexpr float kHistogramHeight = 48.0f;
constexpr float kColorSummaryHeight = 86.0f;
constexpr float kInfoRowCount = 6.0f;
constexpr float kSectionHeaderHeight = 20.0f;
constexpr float kScrollStep = 36.0f;
constexpr float kMinimumPanelHeight = 180.0f;

D2D1_COLOR_F WithOpacity(D2D1_COLOR_F color, float opacity)
{
    color.a = opacity;
    return color;
}

const std::array<unsigned int, 256>& HistogramForChannel(
    const ImagePixelAnalysis& analysis,
    ImgViewerHistogramChannel channel)
{
    switch (channel) {
    case ImgViewerHistogramChannel::Red:
        return analysis.red_histogram;
    case ImgViewerHistogramChannel::Green:
        return analysis.green_histogram;
    case ImgViewerHistogramChannel::Blue:
        return analysis.blue_histogram;
    case ImgViewerHistogramChannel::Luma:
    default:
        return analysis.luma_histogram;
    }
}

const wchar_t* ChannelText(ImgViewerHistogramChannel channel)
{
    switch (channel) {
    case ImgViewerHistogramChannel::Red:
        return L"R";
    case ImgViewerHistogramChannel::Green:
        return L"G";
    case ImgViewerHistogramChannel::Blue:
        return L"B";
    case ImgViewerHistogramChannel::Luma:
    default:
        return L"Luma";
    }
}

D2D1_COLOR_F ChannelColor(ImgViewerHistogramChannel channel)
{
    switch (channel) {
    case ImgViewerHistogramChannel::Red:
        return COLOR(0xd64545);
    case ImgViewerHistogramChannel::Green:
        return COLOR(0x2f9e44);
    case ImgViewerHistogramChannel::Blue:
        return COLOR(0x2f6fed);
    case ImgViewerHistogramChannel::Luma:
    default:
        return ui_theme::color::kMutedText;
    }
}

D2D1_COLOR_F SampleColor(ImageColorSample color)
{
    return D2D1::ColorF(
        static_cast<float>(color.red) / 255.0f,
        static_cast<float>(color.green) / 255.0f,
        static_cast<float>(color.blue) / 255.0f,
        1.0f);
}

std::wstring HexColor(ImageColorSample color)
{
    wchar_t text[8] = {};
    swprintf_s(text, L"#%02X%02X%02X", color.red, color.green, color.blue);
    return text;
}

} // namespace

ImgViewerUiInfoPanel::ImgViewerUiInfoPanel(UiElement& root)
{
    panel_ = root.AddChild(std::make_unique<UiElement>(UiMetadata(
        UiElementRole::Pane,
        kUiActionNone,
        L"Info Panel",
        L"Info Panel",
        L"info-panel",
        false,
        true)));
    panel_id_ = panel_->Id();
    panel_->SetHitTestVisible(false);
}

void ImgViewerUiInfoPanel::SetState(ImgViewerUiInfoPanelState state)
{
    state_ = std::move(state);
    scroll_offset_ = std::clamp(scroll_offset_, 0.0f, MaxScrollOffset());
    if (panel_ != nullptr) {
        panel_->SetHitTestVisible(state_.visible);
    }
}

bool ImgViewerUiInfoPanel::IsVisible() const
{
    return state_.visible;
}

D2D1_SIZE_F ImgViewerUiInfoPanel::Measure(const UiDrawContext&, D2D1_SIZE_F available_size) const
{
    const float width = (std::min)(kPanelWidth, (std::max)(90.0f, available_size.width - kPanelMargin * 2.0f));
    const float top = ui_theme::metrics::kTitleBarHeight + kPanelMargin;
    const float bottom = (std::max)(top + kMinimumPanelHeight, available_size.height - kPanelMargin);
    return D2D1::SizeF(width, bottom - top);
}

void ImgViewerUiInfoPanel::Arrange(D2D1_RECT_F final_rect) const
{
    if (panel_ == nullptr) {
        return;
    }

    const D2D1_SIZE_F size = Measure(UiDrawContext{}, D2D1::SizeF(final_rect.right - final_rect.left, final_rect.bottom - final_rect.top));
    const float left = (std::max)(kPanelMargin, final_rect.right - size.width - kPanelMargin);
    const float top = ui_theme::metrics::kTitleBarHeight + kPanelMargin;
    panel_->Arrange(D2D1::RectF(left, top, left + size.width, top + size.height));
}

void ImgViewerUiInfoPanel::Render(const UiDrawContext& draw_context) const
{
    if (!state_.visible || panel_ == nullptr) {
        return;
    }

    const UiDraw draw(draw_context);
    const D2D1_RECT_F rect = panel_->Rect();
    draw.FillRoundedRect(
        D2D1::RoundedRect(rect, 4.0f, 4.0f),
        WithOpacity(ui_theme::color::kButtonDefault, 0.94f));
    draw.DrawRoundedRect(D2D1::RoundedRect(rect, 4.0f, 4.0f), ui_theme::color::kBorder);

    const D2D1_RECT_F content = D2D1::RectF(
        rect.left + kPanelSidePadding,
        rect.top + kPanelTopPadding,
        rect.right - kPanelSidePadding,
        rect.bottom - kPanelTopPadding);
    draw.DrawBodyText(
        L"Info",
        4,
        D2D1::RectF(content.left, content.top, content.right, content.top + kHeaderHeight),
        ui_theme::color::kBodyText);

    const D2D1_RECT_F body_clip = D2D1::RectF(
        content.left,
        BodyTop(),
        content.right,
        rect.bottom - kPanelTopPadding);
    draw_context.d2d_context->PushAxisAlignedClip(body_clip, D2D1_ANTIALIAS_MODE_ALIASED);

    float top = BodyTop() - EffectiveScrollOffset();
    DrawRow(draw, L"Name", state_.name, top);
    top += kRowHeight;
    DrawRow(draw, L"Path", state_.path, top);
    top += kRowHeight;
    DrawRow(draw, L"Dimensions", state_.dimensions, top);
    top += kRowHeight;
    DrawRow(draw, L"Type", state_.type, top);
    top += kRowHeight;
    DrawRow(draw, L"File size", state_.file_size, top);
    top += kRowHeight;
    DrawRow(draw, L"Modified", state_.modified_time, top);
    top += kRowHeight + kHistogramTopGap;
    if (!state_.exif_rows.empty()) {
        DrawSectionHeader(draw, L"EXIF", top);
        top += kSectionHeaderHeight;
        for (const ImageMetadataRow& row : state_.exif_rows) {
            DrawRow(draw, row.label.c_str(), row.value, top);
            top += kRowHeight;
        }
        top += kHistogramTopGap;
    }
    DrawHistogram(draw, D2D1::RectF(content.left, top, content.right, top + kHistogramHeaderHeight + kHistogramTabHeight + kHistogramHeight));
    top += kHistogramHeaderHeight + kHistogramTabHeight + kHistogramHeight + kHistogramTopGap;
    DrawColorSummary(draw, D2D1::RectF(content.left, top, content.right, top + kColorSummaryHeight));

    draw_context.d2d_context->PopAxisAlignedClip();

    const float max_scroll = MaxScrollOffset();
    if (max_scroll > 0.0f) {
        const float track_left = rect.right - 4.0f;
        const float track_top = body_clip.top;
        const float track_bottom = body_clip.bottom;
        const float track_height = (std::max)(0.5f, track_bottom - track_top);
        const float thumb_height = (std::max)(14.0f, track_height * BodyViewportHeight() / BodyContentHeight());
        const float thumb_top = track_top + (track_height - thumb_height) * EffectiveScrollOffset() / max_scroll;
        draw.FillRoundedRect(
            D2D1::RoundedRect(D2D1::RectF(track_left, thumb_top, track_left + 1.5f, thumb_top + thumb_height), 0.75f, 0.75f),
            WithOpacity(ui_theme::color::kMutedText, 0.35f));
    }
}

UiEventResult ImgViewerUiInfoPanel::OnPointerEvent(const UiPointerEvent& event)
{
    if (!state_.visible || panel_ == nullptr || event.target != panel_id_) {
        return {};
    }

    if (event.type == UiEventType::PointerWheel) {
        return UiEventResult{.handled = true, .needs_render = ScrollByWheelDelta(event.wheel_delta)};
    }

    if (event.type == UiEventType::PointerUp && ToggleHistogramChannelFromPoint(event.point)) {
        return UiEventResult{.handled = true, .needs_render = true, .value_changed = true};
    }

    return UiEventResult{.handled = true};
}

void ImgViewerUiInfoPanel::DrawRow(
    const UiDraw& draw,
    const wchar_t* label,
    const std::wstring& value,
    float top) const
{
    const D2D1_RECT_F panel_rect = panel_ != nullptr ? panel_->Rect() : D2D1::RectF();
    const D2D1_RECT_F row = D2D1::RectF(
        panel_rect.left + kPanelSidePadding,
        top,
        panel_rect.right - kPanelSidePadding,
        top + kRowHeight);
    const wchar_t* display_value = value.empty() ? L"-" : value.c_str();
    const UINT32 display_value_length = value.empty() ? 1 : static_cast<UINT32>(value.size());
    draw.DrawBodyText(
        label,
        static_cast<UINT32>(std::wcslen(label)),
        D2D1::RectF(row.left, row.top + 3.0f, row.left + kLabelWidth, row.bottom),
        ui_theme::color::kMutedText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP);
    draw.DrawBodyText(
        display_value,
        display_value_length,
        D2D1::RectF(row.left + kLabelWidth, row.top + 3.0f, row.right, row.bottom),
        ui_theme::color::kBodyText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void ImgViewerUiInfoPanel::DrawSectionHeader(const UiDraw& draw, const wchar_t* text, float top) const
{
    const D2D1_RECT_F panel_rect = panel_ != nullptr ? panel_->Rect() : D2D1::RectF();
    draw.DrawBodyText(
        text,
        static_cast<UINT32>(std::wcslen(text)),
        D2D1::RectF(
            panel_rect.left + kPanelSidePadding,
            top + 3.0f,
            panel_rect.right - kPanelSidePadding,
            top + kSectionHeaderHeight),
        ui_theme::color::kMutedText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void ImgViewerUiInfoPanel::DrawHistogram(const UiDraw& draw, const D2D1_RECT_F& rect) const
{
    draw.DrawBodyText(
        L"Histogram",
        9,
        D2D1::RectF(rect.left, rect.top, rect.right, rect.top + kHistogramHeaderHeight),
        ui_theme::color::kMutedText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP);

    const D2D1_RECT_F tabs = D2D1::RectF(
        rect.left,
        rect.top + kHistogramHeaderHeight,
        rect.right,
        rect.top + kHistogramHeaderHeight + kHistogramTabHeight);
    DrawHistogramTabs(draw, tabs);

    const D2D1_RECT_F chart = D2D1::RectF(
        rect.left,
        tabs.bottom + 3.0f,
        rect.right,
        tabs.bottom + 3.0f + kHistogramHeight);
    draw.FillRect(chart, COLOR(0xf7f9fc));
    draw.DrawRect(chart, ui_theme::color::kBorder);
    if (!state_.has_analysis) {
        draw.DrawBodyText(
            L"Unavailable",
            11,
            D2D1::RectF(chart.left + 4.0f, chart.top + 12.0f, chart.right - 4.0f, chart.bottom),
            ui_theme::color::kButtonDisabledContent,
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
        return;
    }

    const float chart_width = (std::max)(0.5f, chart.right - chart.left - 1.0f);
    const float chart_height = (std::max)(0.5f, chart.bottom - chart.top - 1.0f);
    constexpr std::array<ImgViewerHistogramChannel, 4> kChannels{
        ImgViewerHistogramChannel::Luma,
        ImgViewerHistogramChannel::Red,
        ImgViewerHistogramChannel::Green,
        ImgViewerHistogramChannel::Blue,
    };
    for (ImgViewerHistogramChannel channel : kChannels) {
        if (!IsHistogramChannelVisible(channel)) {
            continue;
        }

        const std::array<unsigned int, 256>& histogram = HistogramForChannel(state_.analysis, channel);
        const unsigned int max_count = (std::max)(1U, *std::max_element(histogram.begin(), histogram.end()));
        const float bar_width = chart_width / static_cast<float>(histogram.size());
        const D2D1_COLOR_F color = ChannelColor(channel);
        for (size_t index = 0; index < histogram.size(); ++index) {
            if (histogram[index] == 0) {
                continue;
            }

            const float ratio = static_cast<float>(histogram[index]) / static_cast<float>(max_count);
            const float left = chart.left + 0.5f + static_cast<float>(index) * bar_width;
            const float right = (std::max)(left + 0.3f, chart.left + 0.5f + static_cast<float>(index + 1) * bar_width);
            const float bottom = chart.bottom - 0.5f;
            const float top = bottom - chart_height * ratio;
            draw.FillRect(D2D1::RectF(left, top, right, bottom), WithOpacity(color, 0.42f));
        }
    }
}

void ImgViewerUiInfoPanel::DrawHistogramTabs(const UiDraw& draw, const D2D1_RECT_F&) const
{
    constexpr std::array<ImgViewerHistogramChannel, 4> kChannels{
        ImgViewerHistogramChannel::Luma,
        ImgViewerHistogramChannel::Red,
        ImgViewerHistogramChannel::Green,
        ImgViewerHistogramChannel::Blue,
    };
    for (ImgViewerHistogramChannel channel : kChannels) {
        const D2D1_RECT_F rect = HistogramTabRect(channel);
        const bool active = IsHistogramChannelVisible(channel);
        draw.FillRect(rect, active ? ui_theme::color::kButtonPressed : ui_theme::color::kButtonDefault);
        draw.DrawRect(rect, active ? ui_theme::color::kAccent : ui_theme::color::kBorder);
        const wchar_t* text = ChannelText(channel);
        draw.DrawBodyText(
            text,
            static_cast<UINT32>(std::wcslen(text)),
            D2D1::RectF(rect.left + 5.0f, rect.top + 3.5f, rect.right - 5.0f, rect.bottom),
            active ? ChannelColor(channel) : ui_theme::color::kBodyText,
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
}

void ImgViewerUiInfoPanel::DrawColorSummary(const UiDraw& draw, const D2D1_RECT_F& rect) const
{
    draw.DrawBodyText(
        L"Color summary",
        13,
        D2D1::RectF(rect.left, rect.top, rect.right, rect.top + kHistogramHeaderHeight),
        ui_theme::color::kMutedText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP);
    if (!state_.has_analysis) {
        draw.DrawBodyText(
            L"Unavailable",
            11,
            D2D1::RectF(rect.left, rect.top + 12.0f, rect.right, rect.bottom),
            ui_theme::color::kButtonDisabledContent,
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
        return;
    }

    const float gap = 4.0f;
    const float width = (rect.right - rect.left - gap * 2.0f) / 3.0f;
    const float top = rect.top + kHistogramHeaderHeight + 2.0f;
    DrawColorChip(draw, L"Avg", state_.analysis.average, D2D1::RectF(rect.left, top, rect.left + width, rect.bottom));
    DrawColorChip(
        draw,
        L"Dark",
        state_.analysis.darkest,
        D2D1::RectF(rect.left + width + gap, top, rect.left + width * 2.0f + gap, rect.bottom));
    DrawColorChip(
        draw,
        L"Bright",
        state_.analysis.brightest,
        D2D1::RectF(rect.left + width * 2.0f + gap * 2.0f, top, rect.right, rect.bottom));
}

void ImgViewerUiInfoPanel::DrawColorChip(
    const UiDraw& draw,
    const wchar_t* label,
    ImageColorSample color,
    D2D1_RECT_F rect) const
{
    const D2D1_RECT_F chip = D2D1::RectF(rect.left, rect.top, rect.right, rect.top + 17.0f);
    draw.FillRect(chip, SampleColor(color));
    draw.DrawRect(chip, ui_theme::color::kBorder);
    draw.DrawBodyText(
        label,
        static_cast<UINT32>(std::wcslen(label)),
        D2D1::RectF(rect.left, chip.bottom + 4.0f, rect.right, chip.bottom + 23.0f),
        ui_theme::color::kMutedText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP);
    const std::wstring hex = HexColor(color);
    draw.DrawBodyText(
        hex.c_str(),
        static_cast<UINT32>(hex.size()),
        D2D1::RectF(rect.left, chip.bottom + 25.0f, rect.right, rect.bottom),
        ui_theme::color::kBodyText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

bool ImgViewerUiInfoPanel::IsHistogramChannelVisible(ImgViewerHistogramChannel channel) const
{
    return visible_histogram_channels_[static_cast<size_t>(channel)];
}

float ImgViewerUiInfoPanel::BodyContentHeight() const
{
    float height = kRowHeight * kInfoRowCount + kHistogramTopGap;
    if (!state_.exif_rows.empty()) {
        height += kSectionHeaderHeight + kRowHeight * static_cast<float>(state_.exif_rows.size()) + kHistogramTopGap;
    }
    height += kHistogramHeaderHeight + kHistogramTabHeight + kHistogramHeight;
    height += kHistogramTopGap + kColorSummaryHeight;
    return height;
}

float ImgViewerUiInfoPanel::BodyViewportHeight() const
{
    if (panel_ == nullptr) {
        return 0.0f;
    }

    const D2D1_RECT_F rect = panel_->Rect();
    return (std::max)(0.0f, rect.bottom - kPanelTopPadding - BodyTop());
}

float ImgViewerUiInfoPanel::MaxScrollOffset() const
{
    return (std::max)(0.0f, BodyContentHeight() - BodyViewportHeight());
}

float ImgViewerUiInfoPanel::EffectiveScrollOffset() const
{
    return std::clamp(scroll_offset_, 0.0f, MaxScrollOffset());
}

float ImgViewerUiInfoPanel::BodyTop() const
{
    if (panel_ == nullptr) {
        return 0.0f;
    }

    const D2D1_RECT_F rect = panel_->Rect();
    return rect.top + kPanelTopPadding + kHeaderHeight + 4.0f;
}

D2D1_RECT_F ImgViewerUiInfoPanel::HistogramTabRect(ImgViewerHistogramChannel channel) const
{
    const D2D1_RECT_F panel_rect = panel_ != nullptr ? panel_->Rect() : D2D1::RectF();
    const float left = panel_rect.left + kPanelSidePadding;
    const float right = panel_rect.right - kPanelSidePadding;
    const float tab_width = (right - left) / 4.0f;
    float tab_top = BodyTop() - EffectiveScrollOffset() + kRowHeight * kInfoRowCount + kHistogramTopGap;
    if (!state_.exif_rows.empty()) {
        tab_top += kSectionHeaderHeight + kRowHeight * static_cast<float>(state_.exif_rows.size()) + kHistogramTopGap;
    }
    tab_top += kHistogramHeaderHeight;
    const size_t index = static_cast<size_t>(channel);
    return D2D1::RectF(
        left + tab_width * static_cast<float>(index),
        tab_top,
        left + tab_width * static_cast<float>(index + 1),
        tab_top + kHistogramTabHeight);
}

bool ImgViewerUiInfoPanel::ToggleHistogramChannelFromPoint(D2D1_POINT_2F point)
{
    constexpr std::array<ImgViewerHistogramChannel, 4> kChannels{
        ImgViewerHistogramChannel::Luma,
        ImgViewerHistogramChannel::Red,
        ImgViewerHistogramChannel::Green,
        ImgViewerHistogramChannel::Blue,
    };
    for (ImgViewerHistogramChannel channel : kChannels) {
        const D2D1_RECT_F rect = HistogramTabRect(channel);
        if (math::Contains(rect, point)) {
            bool& visible = visible_histogram_channels_[static_cast<size_t>(channel)];
            visible = !visible;
            if (std::none_of(
                    visible_histogram_channels_.begin(),
                    visible_histogram_channels_.end(),
                    [](bool value) { return value; })) {
                visible = true;
            }
            return true;
        }
    }

    return false;
}

bool ImgViewerUiInfoPanel::ScrollByWheelDelta(int wheel_delta)
{
    const float max_scroll = MaxScrollOffset();
    if (max_scroll <= 0.0f || wheel_delta == 0) {
        scroll_offset_ = 0.0f;
        return false;
    }

    const float steps = static_cast<float>(wheel_delta) / static_cast<float>(WHEEL_DELTA);
    const float old_offset = scroll_offset_;
    scroll_offset_ = std::clamp(scroll_offset_ - steps * kScrollStep, 0.0f, max_scroll);
    return scroll_offset_ != old_offset;
}
