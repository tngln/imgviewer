#include "imgviewer.ui.info_panel.hpp"

#include <algorithm>
#include <array>
#include <cwchar>
#include <memory>
#include <utility>

#include <d2d1helper.h>

#include "imgviewer.strings.hpp"
#include "math.hpp"
#include "ui.events.hpp"
#include "ui.theme.hpp"

namespace {

constexpr float kPanelWidth = 320.0f;
constexpr float kHeaderHeight = 22.0f;
constexpr float kRowHeight = 21.0f;
constexpr float kLabelWidth = 112.0f;
constexpr float kHistogramHeaderHeight = 20.0f;
constexpr float kHistogramTabHeight = 21.0f;
constexpr float kHistogramHeight = 48.0f;
constexpr float kColorSummaryHeight = 86.0f;
constexpr float kInfoRowCount = 6.0f;
constexpr float kSectionHeaderHeight = 20.0f;
constexpr float kScrollStep = 36.0f;
constexpr float kMinimumPanelHeight = 180.0f;

constexpr float kScrollbarThumbMinHeight = 14.0f;
constexpr float kScrollbarThumbWidth = 1.5f;
constexpr float kScrollbarThumbCornerRadius = 0.75f;
constexpr float kChartMargin = 1.0f;
constexpr float kHistogramBarMinWidth = 0.3f;
constexpr float kTabTextPadding = 5.0f;
constexpr float kTabTextTop = 3.5f;
constexpr float kColorChipHeight = 17.0f;
constexpr float kChipSectionTopGap = 2.0f;
constexpr float kPanelMinWidth = 90.0f;
constexpr float kPanelBackgroundOpacity = 0.94f;
constexpr float kScrollbarThumbOpacity = 0.35f;
constexpr float kHistogramBarOpacity = 0.42f;

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
        return ImgViewerString(ImgViewerStringId::Luma);
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

std::wstring TableValue(std::wstring value)
{
    return value.empty() ? std::wstring(L"-") : std::move(value);
}

} // namespace

ImgViewerUiInfoPanel::ImgViewerUiInfoPanel(UiElement& root)
{
    panel_ = root.AddChild(std::make_unique<UiElement>(UiMetadata(
        UiElementRole::Pane,
        ImgViewerString(ImgViewerStringId::InfoPanel),
        false,
        true)));
    panel_id_ = panel_->Id();
    panel_->SetHitTestVisible(false);

    basic_table_ = static_cast<Table*>(panel_->AddChild(std::make_unique<Table>(UiMetadata(
        UiElementRole::Pane,
        ImgViewerString(ImgViewerStringId::ImageDetails),
        kUiTooltipFromName,
        false,
        true))));
    basic_table_->SetColumns(std::vector<TableColumn>{
        TableColumn{L"", kLabelWidth, false},
        TableColumn{L"", 0.0f, true},
    });
    basic_table_->SetRowHeight(kRowHeight);
    basic_table_->SetCellPadding(0.0f);
    basic_table_->SetSeparatorsVisible(false);
    basic_table_->SetHitTestVisible(false);

    color_table_ = static_cast<Table*>(panel_->AddChild(std::make_unique<Table>(UiMetadata(
        UiElementRole::Pane,
        ImgViewerString(ImgViewerStringId::ColorAndHdr),
        kUiTooltipFromName,
        false,
        true))));
    color_table_->SetColumns(std::vector<TableColumn>{
        TableColumn{L"", kLabelWidth, false},
        TableColumn{L"", 0.0f, true},
    });
    color_table_->SetRowHeight(kRowHeight);
    color_table_->SetCellPadding(0.0f);
    color_table_->SetSeparatorsVisible(false);
    color_table_->SetHitTestVisible(false);

    exif_table_ = static_cast<Table*>(panel_->AddChild(std::make_unique<Table>(UiMetadata(
        UiElementRole::Pane,
        ImgViewerString(ImgViewerStringId::Exif),
        kUiTooltipFromName,
        false,
        true))));
    exif_table_->SetColumns(std::vector<TableColumn>{
        TableColumn{L"", kLabelWidth, false},
        TableColumn{L"", 0.0f, true},
    });
    exif_table_->SetRowHeight(kRowHeight);
    exif_table_->SetCellPadding(0.0f);
    exif_table_->SetSeparatorsVisible(false);
    exif_table_->SetHitTestVisible(false);
}

void ImgViewerUiInfoPanel::SetState(ImgViewerUiInfoPanelState state)
{
    state_ = std::move(state);
    if (basic_table_ != nullptr) {
        basic_table_->SetRows(std::vector<TableRow>{
            TableRow{.cells = {ImgViewerString(ImgViewerStringId::Name), TableValue(state_.name)}},
            TableRow{.cells = {ImgViewerString(ImgViewerStringId::Path), TableValue(state_.path)}},
            TableRow{.cells = {ImgViewerString(ImgViewerStringId::Dimensions), TableValue(state_.dimensions)}},
            TableRow{.cells = {ImgViewerString(ImgViewerStringId::Type), TableValue(state_.type)}},
            TableRow{.cells = {ImgViewerString(ImgViewerStringId::FileSize), TableValue(state_.file_size)}},
            TableRow{.cells = {ImgViewerString(ImgViewerStringId::Modified), TableValue(state_.modified_time)}},
        });
    }
    if (color_table_ != nullptr) {
        std::vector<TableRow> rows;
        rows.reserve(state_.color_rows.size());
        for (const ImageMetadataRow& row : state_.color_rows) {
            rows.push_back(TableRow{.cells = {row.label, TableValue(row.value)}});
        }
        color_table_->SetRows(std::move(rows));
    }
    if (exif_table_ != nullptr) {
        std::vector<TableRow> rows;
        rows.reserve(state_.exif_rows.size());
        for (const ImageMetadataRow& row : state_.exif_rows) {
            rows.push_back(TableRow{.cells = {row.label, TableValue(row.value)}});
        }
        exif_table_->SetRows(std::move(rows));
    }
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
    const float width = (std::min)(kPanelWidth, (std::max)(kPanelMinWidth, available_size.width - ui_theme::metrics::kStandardGap * 2.0f));
    const float top = ui_theme::metrics::kTitleBarHeight + ui_theme::metrics::kStandardGap;
    const float bottom = (std::max)(top + kMinimumPanelHeight, available_size.height - ui_theme::metrics::kStandardGap);
    return D2D1::SizeF(width, bottom - top);
}

void ImgViewerUiInfoPanel::Arrange(D2D1_RECT_F final_rect) const
{
    if (panel_ == nullptr) {
        return;
    }

    const D2D1_SIZE_F size = Measure(UiDrawContext{}, D2D1::SizeF(final_rect.right - final_rect.left, final_rect.bottom - final_rect.top));
    const float left = (std::max)(ui_theme::metrics::kStandardGap, final_rect.right - size.width - ui_theme::metrics::kStandardGap);
    const float top = ui_theme::metrics::kTitleBarHeight + ui_theme::metrics::kStandardGap;
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
        D2D1::RoundedRect(rect, ui_theme::metrics::kPanelCornerRadius, ui_theme::metrics::kPanelCornerRadius),
        WithOpacity(ui_theme::color::kButtonDefault, kPanelBackgroundOpacity));
    draw.DrawRoundedRect(D2D1::RoundedRect(rect, ui_theme::metrics::kPanelCornerRadius, ui_theme::metrics::kPanelCornerRadius), ui_theme::color::kBorder);

    const D2D1_RECT_F content = D2D1::RectF(
        rect.left + ui_theme::metrics::kSectionPadding,
        rect.top + ui_theme::metrics::kSectionPadding,
        rect.right - ui_theme::metrics::kSectionPadding,
        rect.bottom - ui_theme::metrics::kSectionPadding);
    draw.DrawBodyText(
        ImgViewerString(ImgViewerStringId::Info),
        D2D1::RectF(content.left, content.top, content.right, content.top + kHeaderHeight),
        ui_theme::color::kBodyText);

    const D2D1_RECT_F body_clip = D2D1::RectF(
        content.left,
        BodyTop(),
        content.right,
        rect.bottom - ui_theme::metrics::kSectionPadding);
    draw_context.d2d_context->PushAxisAlignedClip(body_clip, D2D1_ANTIALIAS_MODE_ALIASED);

    float top = BodyTop() - EffectiveScrollOffset();
    const float basic_table_height = kRowHeight * kInfoRowCount;
    if (basic_table_ != nullptr) {
        basic_table_->Arrange(D2D1::RectF(content.left, top, content.right, top + basic_table_height));
        basic_table_->Render(draw_context, UiRootState{});
    }
    top += basic_table_height + ui_theme::metrics::kLargeGap;
    if (!state_.color_rows.empty()) {
        DrawSectionHeader(draw, ImgViewerString(ImgViewerStringId::ColorHdr), top);
        top += kSectionHeaderHeight;
        const float color_table_height = kRowHeight * static_cast<float>(state_.color_rows.size());
        if (color_table_ != nullptr) {
            color_table_->Arrange(D2D1::RectF(content.left, top, content.right, top + color_table_height));
            color_table_->Render(draw_context, UiRootState{});
        }
        top += color_table_height;
        top += ui_theme::metrics::kLargeGap;
    }
    if (!state_.exif_rows.empty()) {
        DrawSectionHeader(draw, ImgViewerString(ImgViewerStringId::Exif), top);
        top += kSectionHeaderHeight;
        const float exif_table_height = kRowHeight * static_cast<float>(state_.exif_rows.size());
        if (exif_table_ != nullptr) {
            exif_table_->Arrange(D2D1::RectF(content.left, top, content.right, top + exif_table_height));
            exif_table_->Render(draw_context, UiRootState{});
        }
        top += exif_table_height;
        top += ui_theme::metrics::kLargeGap;
    }
    DrawHistogram(draw, D2D1::RectF(content.left, top, content.right, top + kHistogramHeaderHeight + kHistogramTabHeight + kHistogramHeight));
    top += kHistogramHeaderHeight + kHistogramTabHeight + kHistogramHeight + ui_theme::metrics::kLargeGap;
    DrawColorSummary(draw, D2D1::RectF(content.left, top, content.right, top + kColorSummaryHeight));

    draw_context.d2d_context->PopAxisAlignedClip();

    const float max_scroll = MaxScrollOffset();
    if (max_scroll > 0.0f) {
        const float track_left = rect.right - ui_theme::metrics::kSmallGap;
        const float track_top = body_clip.top;
        const float track_bottom = body_clip.bottom;
        const float track_height = (std::max)(0.5f, track_bottom - track_top);
        const float thumb_height = (std::max)(kScrollbarThumbMinHeight, track_height * BodyViewportHeight() / BodyContentHeight());
        const float thumb_top = track_top + (track_height - thumb_height) * EffectiveScrollOffset() / max_scroll;
        draw.FillRoundedRect(
            D2D1::RoundedRect(D2D1::RectF(track_left, thumb_top, track_left + kScrollbarThumbWidth, thumb_top + thumb_height), kScrollbarThumbCornerRadius, kScrollbarThumbCornerRadius),
            WithOpacity(ui_theme::color::kMutedText, kScrollbarThumbOpacity));
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

void ImgViewerUiInfoPanel::DrawSectionHeader(const UiDraw& draw, const wchar_t* text, float top) const
{
    const D2D1_RECT_F panel_rect = panel_ != nullptr ? panel_->Rect() : D2D1::RectF();
    draw.DrawBodyText(
        text,
        D2D1::RectF(
            panel_rect.left + ui_theme::metrics::kSectionPadding,
            top + ui_theme::metrics::kTextRowTopOffset,
            panel_rect.right - ui_theme::metrics::kSectionPadding,
            top + kSectionHeaderHeight),
        ui_theme::color::kMutedText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void ImgViewerUiInfoPanel::DrawHistogram(const UiDraw& draw, const D2D1_RECT_F& rect) const
{
    draw.DrawBodyText(
        ImgViewerString(ImgViewerStringId::Histogram),
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
            ImgViewerString(ImgViewerStringId::Unavailable),
            D2D1::RectF(chart.left + 4.0f, chart.top + 12.0f, chart.right - 4.0f, chart.bottom),
            ui_theme::color::kButtonDisabledContent,
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
        return;
    }

    const float chart_width = (std::max)(0.5f, chart.right - chart.left - kChartMargin);
    const float chart_height = (std::max)(0.5f, chart.bottom - chart.top - kChartMargin);
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
            const float left = chart.left + ui_theme::metrics::kHalfPixel + static_cast<float>(index) * bar_width;
            const float right = (std::max)(left + kHistogramBarMinWidth, chart.left + ui_theme::metrics::kHalfPixel + static_cast<float>(index + 1) * bar_width);
            const float bottom = chart.bottom - ui_theme::metrics::kHalfPixel;
            const float top = bottom - chart_height * ratio;
            draw.FillRect(D2D1::RectF(left, top, right, bottom), WithOpacity(color, kHistogramBarOpacity));
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
            D2D1::RectF(rect.left + kTabTextPadding, rect.top + kTabTextTop, rect.right - kTabTextPadding, rect.bottom),
            active ? ChannelColor(channel) : ui_theme::color::kBodyText,
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
}

void ImgViewerUiInfoPanel::DrawColorSummary(const UiDraw& draw, const D2D1_RECT_F& rect) const
{
    draw.DrawBodyText(
        ImgViewerString(ImgViewerStringId::ColorSummary),
        D2D1::RectF(rect.left, rect.top, rect.right, rect.top + kHistogramHeaderHeight),
        ui_theme::color::kMutedText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP);
    if (!state_.has_analysis) {
        draw.DrawBodyText(
            ImgViewerString(ImgViewerStringId::Unavailable),
            D2D1::RectF(rect.left, rect.top + 12.0f, rect.right, rect.bottom),
            ui_theme::color::kButtonDisabledContent,
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
        return;
    }

    const float gap = ui_theme::metrics::kSmallGap;
    const float width = (rect.right - rect.left - gap * 2.0f) / 3.0f;
    const float top = rect.top + kHistogramHeaderHeight + kChipSectionTopGap;
    DrawColorChip(draw, ImgViewerString(ImgViewerStringId::AverageAbbrev), state_.analysis.average, D2D1::RectF(rect.left, top, rect.left + width, rect.bottom));
    DrawColorChip(
        draw,
        ImgViewerString(ImgViewerStringId::Dark),
        state_.analysis.darkest,
        D2D1::RectF(rect.left + width + gap, top, rect.left + width * 2.0f + gap, rect.bottom));
    DrawColorChip(
        draw,
        ImgViewerString(ImgViewerStringId::Bright),
        state_.analysis.brightest,
        D2D1::RectF(rect.left + width * 2.0f + gap * 2.0f, top, rect.right, rect.bottom));
}

void ImgViewerUiInfoPanel::DrawColorChip(
    const UiDraw& draw,
    const wchar_t* label,
    ImageColorSample color,
    D2D1_RECT_F rect) const
{
    const D2D1_RECT_F chip = D2D1::RectF(rect.left, rect.top, rect.right, rect.top + kColorChipHeight);
    draw.FillRect(chip, SampleColor(color));
    draw.DrawRect(chip, ui_theme::color::kBorder);
    draw.DrawBodyText(
        label,
        D2D1::RectF(rect.left, chip.bottom + ui_theme::metrics::kSmallGap, rect.right, chip.bottom + 23.0f),
        ui_theme::color::kMutedText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP);
    const std::wstring hex = HexColor(color);
    draw.DrawBodyText(
        hex,
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
    float height = kRowHeight * kInfoRowCount + ui_theme::metrics::kLargeGap;
    if (!state_.color_rows.empty()) {
        height += kSectionHeaderHeight + kRowHeight * static_cast<float>(state_.color_rows.size()) + ui_theme::metrics::kLargeGap;
    }
    if (!state_.exif_rows.empty()) {
        height += kSectionHeaderHeight + kRowHeight * static_cast<float>(state_.exif_rows.size()) + ui_theme::metrics::kLargeGap;
    }
    height += kHistogramHeaderHeight + kHistogramTabHeight + kHistogramHeight;
    height += ui_theme::metrics::kLargeGap + kColorSummaryHeight;
    return height;
}

float ImgViewerUiInfoPanel::BodyViewportHeight() const
{
    if (panel_ == nullptr) {
        return 0.0f;
    }

    const D2D1_RECT_F rect = panel_->Rect();
    return (std::max)(0.0f, rect.bottom - ui_theme::metrics::kSectionPadding - BodyTop());
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
    return rect.top + ui_theme::metrics::kSectionPadding + kHeaderHeight + ui_theme::metrics::kSmallGap;
}

D2D1_RECT_F ImgViewerUiInfoPanel::HistogramTabRect(ImgViewerHistogramChannel channel) const
{
    const D2D1_RECT_F panel_rect = panel_ != nullptr ? panel_->Rect() : D2D1::RectF();
    const float left = panel_rect.left + ui_theme::metrics::kSectionPadding;
    const float right = panel_rect.right - ui_theme::metrics::kSectionPadding;
    const float tab_width = (right - left) / 4.0f;
    float tab_top = BodyTop() - EffectiveScrollOffset() + kRowHeight * kInfoRowCount + ui_theme::metrics::kLargeGap;
    if (!state_.color_rows.empty()) {
        tab_top += kSectionHeaderHeight + kRowHeight * static_cast<float>(state_.color_rows.size()) + ui_theme::metrics::kLargeGap;
    }
    if (!state_.exif_rows.empty()) {
        tab_top += kSectionHeaderHeight + kRowHeight * static_cast<float>(state_.exif_rows.size()) + ui_theme::metrics::kLargeGap;
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
