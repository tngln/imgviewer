#include "imgviewer.strings.hpp"

#include <array>

namespace {

struct LocalizedString final {
    ImgViewerStringId id = ImgViewerStringId::Empty;
    const wchar_t* en = L"";
};

#define IMGVIEWER_STRINGS(X) \
    X(Empty, L"") \
    X(AppName, L"ImgViewer") \
    X(OpenImage, L"Open Image") \
    X(CaptureRegion, L"Region Screenshot") \
    X(SaveAs, L"Save As") \
    X(ShowInFileExplorer, L"Show in File Explorer") \
    X(PreviousImage, L"Previous Image") \
    X(NextImage, L"Next Image") \
    X(ZoomIn, L"Zoom In") \
    X(ZoomOut, L"Zoom Out") \
    X(FitWindow, L"Fit Window") \
    X(ActualSize, L"Actual Size") \
    X(RotateClockwise, L"Rotate Clockwise") \
    X(FlipHorizontal, L"Flip Horizontal") \
    X(FlipVertical, L"Flip Vertical") \
    X(ResetView, L"Reset View") \
    X(ColorPicker, L"Color Picker") \
    X(CopyColorPickerValue, L"Copy Color Picker Value") \
    X(EditMode, L"Edit Mode") \
    X(ToggleEditMode, L"Toggle Edit Mode") \
    X(EditSelect, L"Edit Select") \
    X(EditPixelSelect, L"Edit Pixel Select") \
    X(PixelSelect, L"Pixel Select") \
    X(EditPen, L"Edit Pen") \
    X(EditShape, L"Edit Shape") \
    X(EditText, L"Edit Text") \
    X(EditCrop, L"Edit Crop") \
    X(EditCancelCrop, L"Cancel Crop") \
    X(EditCopySelection, L"Copy Pixel Selection") \
    X(EditMosaicSelection, L"Mosaic Pixel Selection") \
    X(EditDeleteSelection, L"Delete Edit Selection") \
    X(EditRotateClockwise, L"Edit Rotate Clockwise") \
    X(EditUndo, L"Edit Undo") \
    X(EditRedo, L"Edit Redo") \
    X(ToggleInfoPanel, L"Info Panel") \
    X(InfoPanel, L"Info Panel") \
    X(LoopAnimation, L"Loop Animation") \
    X(PlayAnimation, L"Play Animation") \
    X(PauseAnimation, L"Pause Animation") \
    X(PlayOrPauseAnimation, L"Play or Pause Animation") \
    X(PreviousAnimationFrame, L"Previous Animation Frame") \
    X(NextAnimationFrame, L"Next Animation Frame") \
    X(Menu, L"Menu") \
    X(Settings, L"Settings") \
    X(CloseSettings, L"Close Settings") \
    X(SaveSettings, L"Save Settings") \
    X(Developer, L"Developer") \
    X(CloseDeveloper, L"Close Developer") \
    X(DeveloperSampleButton, L"Developer Sample Button") \
    X(About, L"About") \
    X(AboutImgViewer, L"About ImgViewer") \
    X(CloseAbout, L"Close About") \
    X(ResetShortcuts, L"Reset Shortcuts") \
    X(Copy, L"Copy") \
    X(Cut, L"Cut") \
    X(Paste, L"Paste") \
    X(SelectAll, L"Select All") \
    X(TopMost, L"Top Most") \
    X(Minimize, L"Minimize") \
    X(MaximizeOrRestore, L"Maximize or Restore") \
    X(Close, L"Close") \
    X(KeepWindowOnTop, L"Keep window on top") \
    X(PreviousImageTooltip, L"Previous image") \
    X(NextImageTooltip, L"Next image") \
    X(CaptureRegionTooltip, L"Region screenshot") \
    X(ZoomInTooltip, L"Zoom in") \
    X(ZoomOutTooltip, L"Zoom out") \
    X(FitWindowTooltip, L"Fit window") \
    X(ActualSizeTooltip, L"Actual size") \
    X(RotateClockwiseTooltip, L"Rotate clockwise") \
    X(FlipHorizontalTooltip, L"Flip horizontal") \
    X(FlipVerticalTooltip, L"Flip vertical") \
    X(ResetViewTooltip, L"Reset view") \
    X(ColorPickerTooltip, L"Color picker") \
    X(ToolbarButtons, L"Toolbar buttons") \
    X(ToolbarDragHandle, L"Toolbar drag handle") \
    X(CaptionButtons, L"Caption buttons") \
    X(EditToolbar, L"Edit toolbar") \
    X(SelectEditObjects, L"Select edit objects") \
    X(SelectPixels, L"Select pixels") \
    X(PenAnnotation, L"Pen annotation") \
    X(ShapeAnnotation, L"Shape annotation") \
    X(TextAnnotation, L"Text annotation") \
    X(CropImage, L"Crop image") \
    X(RotateEditClockwise, L"Rotate edit clockwise") \
    X(UndoEdit, L"Undo edit") \
    X(RedoEdit, L"Redo edit") \
    X(SaveEditedImageAsPng, L"Save edited image as PNG") \
    X(ExitEditMode, L"Exit edit mode") \
    X(PenTools, L"Pen tools") \
    X(ShapeTools, L"Shape tools") \
    X(TextTools, L"Text tools") \
    X(PixelSelectionTools, L"Pixel selection tools") \
    X(ColorPickerTools, L"Color picker tools") \
    X(AnimationControls, L"Animation controls") \
    X(Red, L"Red") \
    X(Yellow, L"Yellow") \
    X(Green, L"Green") \
    X(Cyan, L"Cyan") \
    X(Blue, L"Blue") \
    X(Magenta, L"Magenta") \
    X(White, L"White") \
    X(Black, L"Black") \
    X(None, L"None") \
    X(Rectangle, L"Rectangle") \
    X(Ellipse, L"Ellipse") \
    X(Line, L"Line") \
    X(Arrow, L"Arrow") \
    X(RedPen, L"Red pen") \
    X(YellowPen, L"Yellow pen") \
    X(GreenPen, L"Green pen") \
    X(CyanPen, L"Cyan pen") \
    X(BluePen, L"Blue pen") \
    X(MagentaPen, L"Magenta pen") \
    X(WhitePen, L"White pen") \
    X(BlackPen, L"Black pen") \
    X(PenWidth2, L"2 px pen") \
    X(PenWidth4, L"4 px pen") \
    X(PenWidth8, L"8 px pen") \
    X(PenWidth12, L"12 px pen") \
    X(RectangleShape, L"Rectangle shape") \
    X(EllipseShape, L"Ellipse shape") \
    X(LineShape, L"Line shape") \
    X(ArrowShape, L"Arrow shape") \
    X(RedShape, L"Red shape") \
    X(YellowShape, L"Yellow shape") \
    X(GreenShape, L"Green shape") \
    X(CyanShape, L"Cyan shape") \
    X(BlueShape, L"Blue shape") \
    X(MagentaShape, L"Magenta shape") \
    X(WhiteShape, L"White shape") \
    X(BlackShape, L"Black shape") \
    X(TextFont, L"Text font") \
    X(Font, L"Font") \
    X(TextSize12, L"Text Size 12px") \
    X(TextSize16, L"Text Size 16px") \
    X(TextSize20, L"Text Size 20px") \
    X(TextSize28, L"Text Size 28px") \
    X(TextSize36, L"Text Size 36px") \
    X(TextColorRed, L"Text Color Red") \
    X(TextColorYellow, L"Text Color Yellow") \
    X(TextColorGreen, L"Text Color Green") \
    X(TextColorCyan, L"Text Color Cyan") \
    X(TextColorBlue, L"Text Color Blue") \
    X(TextColorMagenta, L"Text Color Magenta") \
    X(TextColorWhite, L"Text Color White") \
    X(TextColorBlack, L"Text Color Black") \
    X(TextBackgroundTransparent, L"Text Background Transparent") \
    X(TextBackgroundYellow, L"Text Background Yellow") \
    X(TextBackgroundWhite, L"Text Background White") \
    X(TextBackgroundBlack, L"Text Background Black") \
    X(TextBackgroundRed, L"Text Background Red") \
    X(TextBackgroundBlue, L"Text Background Blue") \
    X(Text12PxTooltip, L"12 px text") \
    X(Text16PxTooltip, L"16 px text") \
    X(Text20PxTooltip, L"20 px text") \
    X(Text28PxTooltip, L"28 px text") \
    X(Text36PxTooltip, L"36 px text") \
    X(RedText, L"Red text") \
    X(YellowText, L"Yellow text") \
    X(GreenText, L"Green text") \
    X(CyanText, L"Cyan text") \
    X(BlueText, L"Blue text") \
    X(MagentaText, L"Magenta text") \
    X(WhiteText, L"White text") \
    X(BlackText, L"Black text") \
    X(TransparentTextBackground, L"Transparent text background") \
    X(YellowTextBackground, L"Yellow text background") \
    X(WhiteTextBackground, L"White text background") \
    X(BlackTextBackground, L"Black text background") \
    X(RedTextBackground, L"Red text background") \
    X(BlueTextBackground, L"Blue text background") \
    X(CopySelection, L"Copy Selection") \
    X(CopySelectedPixels, L"Copy selected pixels") \
    X(MosaicSelection, L"Mosaic Selection") \
    X(MosaicSelectedPixels, L"Mosaic selected pixels") \
    X(ColorValue, L"Color value") \
    X(AnimationFrame, L"Animation frame") \
    X(WindowSize, L"Window size") \
    X(RememberWindowSize, L"Remember window size") \
    X(RememberLastSize, L"Remember last size") \
    X(UseDefaultSize, L"Use default size") \
    X(ImageRendering, L"Image rendering") \
    X(InitialImageView, L"Initial image view") \
    X(PixelatedSampling, L"Pixelated sampling") \
    X(CheckerboardBackground, L"Checkerboard background") \
    X(WindowFrame, L"Window frame") \
    X(BorderlessWindow, L"Borderless window") \
    X(Opacity, L"Opacity") \
    X(ToolbarSize, L"Toolbar size") \
    X(Navigation, L"Navigation") \
    X(EdgeClickNavigation, L"Edge click navigation") \
    X(EdgeClickZone, L"Edge click zone") \
    X(ShortcutFilter, L"Shortcut filter") \
    X(FilterActions, L"Filter actions") \
    X(ActionShortcuts, L"Action shortcuts") \
    X(Action, L"Action") \
    X(Shortcut, L"Shortcut") \
    X(NoShortcutConfigured, L"No shortcut configured.") \
    X(NoMatches, L"No matches") \
    X(Reset, L"Reset") \
    X(Save, L"Save") \
    X(Cancel, L"Cancel") \
    X(ImageDetails, L"Image details") \
    X(ColorAndHdr, L"Color and HDR") \
    X(Exif, L"EXIF") \
    X(Info, L"Info") \
    X(Name, L"Name") \
    X(Path, L"Path") \
    X(Dimensions, L"Dimensions") \
    X(Type, L"Type") \
    X(FileSize, L"File size") \
    X(Modified, L"Modified") \
    X(ColorHdr, L"Color / HDR") \
    X(Histogram, L"Histogram") \
    X(Unavailable, L"Unavailable") \
    X(ColorSummary, L"Color summary") \
    X(AverageAbbrev, L"Avg") \
    X(Dark, L"Dark") \
    X(Bright, L"Bright") \
    X(Luma, L"Luma") \
    X(ClipboardImage, L"Clipboard image") \
    X(Yes, L"Yes") \
    X(No, L"No") \
    X(Container, L"Container") \
    X(SourcePixels, L"Source pixels") \
    X(WicFormat, L"WIC format") \
    X(IccProfile, L"ICC profile") \
    X(Primaries, L"Primaries") \
    X(Transfer, L"Transfer") \
    X(HdrMetadata, L"HDR metadata") \
    X(HdrSource, L"HDR source") \
    X(DisplayPath, L"Display path") \
    X(SourcePreserved, L"Source preserved") \
    X(DateTaken, L"Date taken") \
    X(CameraMake, L"Camera make") \
    X(CameraModel, L"Camera model") \
    X(Lens, L"Lens") \
    X(FocalLength, L"Focal length") \
    X(Aperture, L"Aperture") \
    X(ExposureTime, L"Exposure time") \
    X(Iso, L"ISO") \
    X(ExposureBias, L"Exposure bias") \
    X(Orientation, L"Orientation") \
    X(OrientationNormal, L"Normal") \
    X(OrientationMirroredHorizontal, L"Mirrored horizontal") \
    X(OrientationRotated180, L"Rotated 180") \
    X(OrientationMirroredVertical, L"Mirrored vertical") \
    X(OrientationMirroredHorizontalRotated270, L"Mirrored horizontal, rotated 270") \
    X(OrientationRotated90, L"Rotated 90") \
    X(OrientationMirroredHorizontalRotated90, L"Mirrored horizontal, rotated 90") \
    X(OrientationRotated270, L"Rotated 270") \
    X(DevelopmentBuild, L"Development build") \
    X(AboutDescription, L"Lightweight native image viewer.") \
    X(ThirdPartyNotices, L"Third-party notices") \
    X(EditModeOn, L"Edit mode on.") \
    X(EditModeOff, L"Edit mode off.") \
    X(CouldNotShowFileInExplorer, L"Could not show file in Explorer.") \
    X(CouldNotCopyColor, L"Could not copy color.") \
    X(CopiedSelectedPixels, L"Copied selected pixels.") \
    X(CouldNotCopySelectedPixels, L"Could not copy selected pixels.") \
    X(CouldNotCaptureRegion, L"Could not capture region.") \
    X(SavedSdrCopyHdrSourcePreserved, L"Saved SDR copy. HDR source preserved.") \
    X(SavedImage, L"Saved image.") \
    X(ClipboardDoesNotContainImageOrPath, L"Clipboard does not contain an image or path.") \
    X(CouldNotPasteClipboardImage, L"Could not paste clipboard image.") \
    X(CouldNotOpenSelectedImage, L"Could not open the selected image.") \
    X(CouldNotReadImageFolder, L"Could not read the image folder.") \
    X(CouldNotShowImagePicker, L"Could not show the image picker.") \
    X(CouldNotShowSaveDialog, L"Could not show the save dialog.") \
    X(CouldNotSaveImage, L"Could not save the image.") \
    X(SavedSdrAnnotationExportHdrSourcePreserved, L"Saved SDR annotation export. HDR source preserved.") \
    X(NoImage, L"No image") \
    X(ScreenshotImage, L"Screenshot image") \
    X(ImagesFilter, L"Images") \
    X(AllFilesFilter, L"All files") \
    X(PngImageFilter, L"PNG image") \
    X(DeveloperControlLab, L"Control Lab") \
    X(DeveloperState, L"Developer state") \
    X(Value, L"Value") \
    X(Tool, L"Tool") \
    X(Size, L"Size") \
    X(Color, L"Color") \
    X(Background, L"Background") \
    X(Transparent, L"Transparent") \
    X(CropSelection, L"Crop / Selection") \
    X(DeleteSelection, L"Delete Selection") \
    X(History, L"History")

constexpr std::array kStrings{
#define IMGVIEWER_STRING_ENTRY(id, en) LocalizedString{ImgViewerStringId::id, en},
    IMGVIEWER_STRINGS(IMGVIEWER_STRING_ENTRY)
#undef IMGVIEWER_STRING_ENTRY
};

const LocalizedString* FindString(ImgViewerStringId id)
{
    for (const LocalizedString& entry : kStrings) {
        if (entry.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace

const wchar_t* ImgViewerString(ImgViewerStringId id)
{
    const LocalizedString* entry = FindString(id);
    if (entry == nullptr) {
        return L"";
    }
    return entry->en;
}
