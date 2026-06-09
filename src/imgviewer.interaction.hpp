#pragma once

enum class ImgViewerInteractionMode {
    Viewing,
    Editing,
};

enum class ImgViewerCanvasOwner {
    None,
    Viewer,
    EditTool,
    ColorPicker,
};

enum class ImgViewerKeyboardOwner {
    ViewerShortcut,
    UiFocus,
    Popup,
    EditText,
};

enum class ImgViewerPointerCaptureOwner {
    None,
    Ui,
    ViewerPan,
    ViewerRotate,
    EdgeClickNavigation,
    EditStroke,
    EditCrop,
    EditPixelSelection,
    ColorPicker,
};

enum class ImgViewerModalOwner {
    None,
    Popup,
    ScreenCapture,
    Settings,
    About,
    Developer,
};

class ImgViewerInteractionState final {
public:
    ImgViewerInteractionMode Mode() const;
    ImgViewerCanvasOwner CanvasOwner() const;
    ImgViewerKeyboardOwner KeyboardOwner() const;
    ImgViewerPointerCaptureOwner PointerCapture() const;
    ImgViewerModalOwner Modal() const;

    bool IsEditing() const;
    bool HasPointerCapture() const;
    bool HasModal() const;

    void EnterViewing();
    void EnterEditing();
    void SetCanvasOwner(ImgViewerCanvasOwner owner);
    void BeginColorPick();
    void EndColorPick();
    void SetKeyboardOwner(ImgViewerKeyboardOwner owner);
    void BeginPointerCapture(ImgViewerPointerCaptureOwner owner);
    void EndPointerCapture(ImgViewerPointerCaptureOwner owner);
    void ClearPointerCapture();
    void SetModal(ImgViewerModalOwner owner);
    void ClearModal(ImgViewerModalOwner owner);
    void ResetTransientInput();

private:
    ImgViewerInteractionMode mode_ = ImgViewerInteractionMode::Viewing;
    ImgViewerCanvasOwner canvas_owner_ = ImgViewerCanvasOwner::Viewer;
    ImgViewerKeyboardOwner keyboard_owner_ = ImgViewerKeyboardOwner::ViewerShortcut;
    ImgViewerPointerCaptureOwner pointer_capture_ = ImgViewerPointerCaptureOwner::None;
    ImgViewerModalOwner modal_ = ImgViewerModalOwner::None;
};
