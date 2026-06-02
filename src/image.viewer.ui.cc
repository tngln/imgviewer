#include "image.viewer.ui.hpp"

#include <memory>

#include <d2d1helper.h>

namespace {

UiElementMetadata Metadata(
    UiElementId id,
    UiElementRole role,
    AppAction action,
    const wchar_t* name,
    const wchar_t* tooltip,
    const wchar_t* automation_id,
    bool is_control = true,
    bool is_content = true)
{
    return UiElementMetadata{
        .id = id,
        .role = role,
        .action = action,
        .name = name,
        .tooltip = tooltip,
        .automation_id = automation_id,
        .is_control = is_control,
        .is_content = is_content,
    };
}

} // namespace

ImageViewerUi::ImageViewerUi() :
    root_(std::make_unique<UiElement>(
        Metadata(UiElementId::None, UiElementRole::Pane, AppAction::None, L"ImgViewer", L"", L"root"))),
    titlebar_(*root_, ids_),
    toolbar_(*root_, ids_)
{
}

UiElement* ImageViewerUi::Root()
{
    return root_.get();
}

const UiElement* ImageViewerUi::Root() const
{
    return root_.get();
}

void ImageViewerUi::Draw(
    ID2D1DeviceContext* d2d_context,
    D2D1_SIZE_F viewport_size,
    IDWriteFactory* dwrite_factory,
    IDWriteTextFormat* body_text_format,
    IDWriteTextFormat* icon_text_format,
    ImageViewerUiState state)
{
    const UiDrawContext draw_context{
        .d2d_context = d2d_context,
        .body_text_format = body_text_format,
        .icon_text_format = icon_text_format,
    };
    Layout(viewport_size);
    titlebar_.Draw(draw_context, viewport_size, dwrite_factory, body_text_format, state, top_most_, maximized_);
    toolbar_.Draw(draw_context, viewport_size, state);
}

UiEventResult ImageViewerUi::OnPointerEvent(const UiPointerEvent& event)
{
    return toolbar_.OnPointerEvent(event);
}

bool ImageViewerUi::IsPointInCaptionDragArea(D2D1_POINT_2F point) const
{
    return titlebar_.IsPointInCaptionDragArea(*root_, point);
}

void ImageViewerUi::SetTitleText(const wchar_t* title)
{
    titlebar_.SetTitleText(title);
}

void ImageViewerUi::SetWindowState(bool top_most, bool maximized)
{
    top_most_ = top_most;
    maximized_ = maximized;
}

void ImageViewerUi::Layout(D2D1_SIZE_F viewport_size)
{
    root_->SetRect(D2D1::RectF(0.0f, 0.0f, viewport_size.width, viewport_size.height));
}
