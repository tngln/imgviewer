#include "imgviewer.strings.hpp"

#include <array>
#include <atomic>

namespace {

struct LocalizedString final {
    ImgViewerStringId id = ImgViewerStringId::Empty;
    const wchar_t* en = L"";
    const wchar_t* zh = L"";
};

#define IMGVIEWER_STRINGS(X) \
    X(Empty, L"", L"") \
    X(AppName, L"ImgViewer", L"ImgViewer") \
    X(OpenImage, L"Open Image", L"打开图像") \
    X(CaptureRegion, L"Region Screenshot", L"区域截图") \
    X(SaveAs, L"Save As", L"另存为") \
    X(ShowInFileExplorer, L"Show in File Explorer", L"在文件资源管理器中显示") \
    X(PreviousImage, L"Previous Image", L"上一张图像") \
    X(NextImage, L"Next Image", L"下一张图像") \
    X(ZoomIn, L"Zoom In", L"放大") \
    X(ZoomOut, L"Zoom Out", L"缩小") \
    X(FitWindow, L"Fit Window", L"适应窗口") \
    X(ActualSize, L"Actual Size", L"实际大小") \
    X(RotateClockwise, L"Rotate Clockwise", L"顺时针旋转") \
    X(FlipHorizontal, L"Flip Horizontal", L"水平翻转") \
    X(FlipVertical, L"Flip Vertical", L"垂直翻转") \
    X(ResetView, L"Reset View", L"重置视图") \
    X(ColorPicker, L"Color Picker", L"取色器") \
    X(CopyColorPickerValue, L"Copy Color Picker Value", L"复制取色器值") \
    X(EditMode, L"Edit Mode", L"编辑模式") \
    X(ToggleEditMode, L"Toggle Edit Mode", L"切换编辑模式") \
    X(EditSelect, L"Edit Select", L"编辑选择") \
    X(EditPixelSelect, L"Edit Pixel Select", L"编辑像素选择") \
    X(PixelSelect, L"Pixel Select", L"像素选择") \
    X(EditPen, L"Edit Pen", L"编辑画笔") \
    X(EditShape, L"Edit Shape", L"编辑形状") \
    X(EditText, L"Edit Text", L"编辑文字") \
    X(EditCrop, L"Edit Crop", L"编辑裁剪") \
    X(EditCancelCrop, L"Cancel Crop", L"取消裁剪") \
    X(EditCopySelection, L"Copy Pixel Selection", L"复制像素选区") \
    X(EditMosaicSelection, L"Mosaic Pixel Selection", L"马赛克像素选区") \
    X(EditDeleteSelection, L"Delete Edit Selection", L"删除编辑选区") \
    X(EditRotateClockwise, L"Edit Rotate Clockwise", L"编辑顺时针旋转") \
    X(EditUndo, L"Edit Undo", L"撤销编辑") \
    X(EditRedo, L"Edit Redo", L"重做编辑") \
    X(ToggleInfoPanel, L"Info Panel", L"信息面板") \
    X(InfoPanel, L"Info Panel", L"信息面板") \
    X(LoopAnimation, L"Loop Animation", L"循环动画") \
    X(PlayAnimation, L"Play Animation", L"播放动画") \
    X(PauseAnimation, L"Pause Animation", L"暂停动画") \
    X(PlayOrPauseAnimation, L"Play or Pause Animation", L"播放或暂停动画") \
    X(PreviousAnimationFrame, L"Previous Animation Frame", L"上一动画帧") \
    X(NextAnimationFrame, L"Next Animation Frame", L"下一动画帧") \
    X(Menu, L"Menu", L"菜单") \
    X(OpenMenu, L"Menu", L"菜单") \
    X(Settings, L"Settings", L"设置") \
    X(CloseSettings, L"Close Settings", L"关闭设置") \
    X(SaveSettings, L"Save Settings", L"保存设置") \
    X(Developer, L"Developer", L"开发者") \
    X(CloseDeveloper, L"Close Developer", L"关闭开发者") \
    X(DeveloperSampleButton, L"Developer Sample Button", L"开发者示例按钮") \
    X(About, L"About", L"关于") \
    X(AboutImgViewer, L"About ImgViewer", L"关于 ImgViewer") \
    X(CloseAbout, L"Close About", L"关闭关于") \
    X(CopyAboutNotices, L"Copy About Notices", L"复制关于声明") \
    X(CopyNotices, L"Copy Notices", L"复制声明") \
    X(ResetShortcuts, L"Reset Shortcuts", L"重置快捷键") \
    X(Copy, L"Copy", L"复制") \
    X(Cut, L"Cut", L"剪切") \
    X(Paste, L"Paste", L"粘贴") \
    X(SelectAll, L"Select All", L"全选") \
    X(TopMost, L"Top Most", L"置顶") \
    X(Minimize, L"Minimize", L"最小化") \
    X(MaximizeOrRestore, L"Maximize or Restore", L"最大化或还原") \
    X(Close, L"Close", L"关闭") \
    X(OpenMenuTooltip, L"Open menu", L"打开菜单") \
    X(KeepWindowOnTop, L"Keep window on top", L"保持窗口置顶") \
    X(PreviousImageTooltip, L"Previous image", L"上一张图像") \
    X(NextImageTooltip, L"Next image", L"下一张图像") \
    X(CaptureRegionTooltip, L"Region screenshot", L"区域截图") \
    X(ZoomInTooltip, L"Zoom in", L"放大") \
    X(ZoomOutTooltip, L"Zoom out", L"缩小") \
    X(FitWindowTooltip, L"Fit window", L"适应窗口") \
    X(ActualSizeTooltip, L"Actual size", L"实际大小") \
    X(RotateClockwiseTooltip, L"Rotate clockwise", L"顺时针旋转") \
    X(FlipHorizontalTooltip, L"Flip horizontal", L"水平翻转") \
    X(FlipVerticalTooltip, L"Flip vertical", L"垂直翻转") \
    X(ResetViewTooltip, L"Reset view", L"重置视图") \
    X(ColorPickerTooltip, L"Color picker", L"取色器") \
    X(ToolbarButtons, L"Toolbar buttons", L"工具栏按钮") \
    X(ToolbarDragHandle, L"Toolbar drag handle", L"工具栏拖动柄") \
    X(CaptionButtons, L"Caption buttons", L"标题栏按钮") \
    X(EditToolbar, L"Edit toolbar", L"编辑工具栏") \
    X(SelectEditObjects, L"Select edit objects", L"选择编辑对象") \
    X(SelectPixels, L"Select pixels", L"选择像素") \
    X(PenAnnotation, L"Pen annotation", L"画笔标注") \
    X(ShapeAnnotation, L"Shape annotation", L"形状标注") \
    X(TextAnnotation, L"Text annotation", L"文字标注") \
    X(CropImage, L"Crop image", L"裁剪图像") \
    X(RotateEditClockwise, L"Rotate edit clockwise", L"顺时针旋转编辑") \
    X(UndoEdit, L"Undo edit", L"撤销编辑") \
    X(RedoEdit, L"Redo edit", L"重做编辑") \
    X(SaveEditedImageAsPng, L"Save edited image as PNG", L"将编辑后的图像保存为 PNG") \
    X(ExitEditMode, L"Exit edit mode", L"退出编辑模式") \
    X(PenTools, L"Pen tools", L"画笔工具") \
    X(ShapeTools, L"Shape tools", L"形状工具") \
    X(TextTools, L"Text tools", L"文字工具") \
    X(PixelSelectionTools, L"Pixel selection tools", L"像素选区工具") \
    X(ColorPickerTools, L"Color picker tools", L"取色器工具") \
    X(AnimationControls, L"Animation controls", L"动画控制") \
    X(Red, L"Red", L"红色") \
    X(Yellow, L"Yellow", L"黄色") \
    X(Green, L"Green", L"绿色") \
    X(Cyan, L"Cyan", L"青色") \
    X(Blue, L"Blue", L"蓝色") \
    X(Magenta, L"Magenta", L"品红") \
    X(White, L"White", L"白色") \
    X(Black, L"Black", L"黑色") \
    X(None, L"None", L"无") \
    X(Rectangle, L"Rectangle", L"矩形") \
    X(Ellipse, L"Ellipse", L"椭圆") \
    X(Line, L"Line", L"直线") \
    X(Arrow, L"Arrow", L"箭头") \
    X(RedPen, L"Red pen", L"红色画笔") \
    X(YellowPen, L"Yellow pen", L"黄色画笔") \
    X(GreenPen, L"Green pen", L"绿色画笔") \
    X(CyanPen, L"Cyan pen", L"青色画笔") \
    X(BluePen, L"Blue pen", L"蓝色画笔") \
    X(MagentaPen, L"Magenta pen", L"品红画笔") \
    X(WhitePen, L"White pen", L"白色画笔") \
    X(BlackPen, L"Black pen", L"黑色画笔") \
    X(PenWidth2, L"2 px pen", L"2 像素画笔") \
    X(PenWidth4, L"4 px pen", L"4 像素画笔") \
    X(PenWidth8, L"8 px pen", L"8 像素画笔") \
    X(PenWidth12, L"12 px pen", L"12 像素画笔") \
    X(RectangleShape, L"Rectangle shape", L"矩形形状") \
    X(EllipseShape, L"Ellipse shape", L"椭圆形状") \
    X(LineShape, L"Line shape", L"直线形状") \
    X(ArrowShape, L"Arrow shape", L"箭头形状") \
    X(RedShape, L"Red shape", L"红色形状") \
    X(YellowShape, L"Yellow shape", L"黄色形状") \
    X(GreenShape, L"Green shape", L"绿色形状") \
    X(CyanShape, L"Cyan shape", L"青色形状") \
    X(BlueShape, L"Blue shape", L"蓝色形状") \
    X(MagentaShape, L"Magenta shape", L"品红形状") \
    X(WhiteShape, L"White shape", L"白色形状") \
    X(BlackShape, L"Black shape", L"黑色形状") \
    X(TextFont, L"Text font", L"文字字体") \
    X(Font, L"Font", L"字体") \
    X(TextSize12, L"Text Size 12px", L"文字大小 12 像素") \
    X(TextSize16, L"Text Size 16px", L"文字大小 16 像素") \
    X(TextSize20, L"Text Size 20px", L"文字大小 20 像素") \
    X(TextSize28, L"Text Size 28px", L"文字大小 28 像素") \
    X(TextSize36, L"Text Size 36px", L"文字大小 36 像素") \
    X(TextColorRed, L"Text Color Red", L"文字颜色红色") \
    X(TextColorYellow, L"Text Color Yellow", L"文字颜色黄色") \
    X(TextColorGreen, L"Text Color Green", L"文字颜色绿色") \
    X(TextColorCyan, L"Text Color Cyan", L"文字颜色青色") \
    X(TextColorBlue, L"Text Color Blue", L"文字颜色蓝色") \
    X(TextColorMagenta, L"Text Color Magenta", L"文字颜色品红") \
    X(TextColorWhite, L"Text Color White", L"文字颜色白色") \
    X(TextColorBlack, L"Text Color Black", L"文字颜色黑色") \
    X(TextBackgroundTransparent, L"Text Background Transparent", L"文字背景透明") \
    X(TextBackgroundYellow, L"Text Background Yellow", L"文字背景黄色") \
    X(TextBackgroundWhite, L"Text Background White", L"文字背景白色") \
    X(TextBackgroundBlack, L"Text Background Black", L"文字背景黑色") \
    X(TextBackgroundRed, L"Text Background Red", L"文字背景红色") \
    X(TextBackgroundBlue, L"Text Background Blue", L"文字背景蓝色") \
    X(Text12PxTooltip, L"12 px text", L"12 像素文字") \
    X(Text16PxTooltip, L"16 px text", L"16 像素文字") \
    X(Text20PxTooltip, L"20 px text", L"20 像素文字") \
    X(Text28PxTooltip, L"28 px text", L"28 像素文字") \
    X(Text36PxTooltip, L"36 px text", L"36 像素文字") \
    X(RedText, L"Red text", L"红色文字") \
    X(YellowText, L"Yellow text", L"黄色文字") \
    X(GreenText, L"Green text", L"绿色文字") \
    X(CyanText, L"Cyan text", L"青色文字") \
    X(BlueText, L"Blue text", L"蓝色文字") \
    X(MagentaText, L"Magenta text", L"品红文字") \
    X(WhiteText, L"White text", L"白色文字") \
    X(BlackText, L"Black text", L"黑色文字") \
    X(TransparentTextBackground, L"Transparent text background", L"透明文字背景") \
    X(YellowTextBackground, L"Yellow text background", L"黄色文字背景") \
    X(WhiteTextBackground, L"White text background", L"白色文字背景") \
    X(BlackTextBackground, L"Black text background", L"黑色文字背景") \
    X(RedTextBackground, L"Red text background", L"红色文字背景") \
    X(BlueTextBackground, L"Blue text background", L"蓝色文字背景") \
    X(CopySelection, L"Copy Selection", L"复制选区") \
    X(CopySelectedPixels, L"Copy selected pixels", L"复制选中像素") \
    X(MosaicSelection, L"Mosaic Selection", L"马赛克选区") \
    X(MosaicSelectedPixels, L"Mosaic selected pixels", L"马赛克选中像素") \
    X(ColorValue, L"Color value", L"颜色值") \
    X(AnimationFrame, L"Animation frame", L"动画帧") \
    X(Language, L"Language", L"语言") \
    X(EnglishLanguage, L"English", L"英语") \
    X(SimplifiedChineseLanguage, L"Simplified Chinese", L"简体中文") \
    X(WindowSize, L"Window size", L"窗口大小") \
    X(RememberWindowSize, L"Remember window size", L"记住窗口大小") \
    X(RememberLastSize, L"Remember last size", L"记住上次大小") \
    X(UseDefaultSize, L"Use default size", L"使用默认大小") \
    X(ImageRendering, L"Image rendering", L"图像渲染") \
    X(InitialImageView, L"Initial image view", L"初始图像视图") \
    X(PixelatedSampling, L"Pixelated sampling", L"像素化采样") \
    X(CheckerboardBackground, L"Checkerboard background", L"棋盘格背景") \
    X(WindowFrame, L"Window frame", L"窗口边框") \
    X(BorderlessWindow, L"Borderless window", L"无边框窗口") \
    X(Opacity, L"Opacity", L"不透明度") \
    X(ToolbarSize, L"Toolbar size", L"工具栏大小") \
    X(Navigation, L"Navigation", L"导航") \
    X(EdgeClickNavigation, L"Edge click navigation", L"边缘点击导航") \
    X(EdgeClickZone, L"Edge click zone", L"边缘点击区域") \
    X(ShortcutFilter, L"Shortcut filter", L"快捷键筛选") \
    X(FilterActions, L"Filter actions", L"筛选动作") \
    X(ActionShortcuts, L"Action shortcuts", L"动作快捷键") \
    X(Action, L"Action", L"动作") \
    X(Shortcut, L"Shortcut", L"快捷键") \
    X(NoShortcutConfigured, L"No shortcut configured.", L"未配置快捷键。") \
    X(NoMatches, L"No matches", L"无匹配项") \
    X(Reset, L"Reset", L"重置") \
    X(Save, L"Save", L"保存") \
    X(Cancel, L"Cancel", L"取消") \
    X(TableCellEditor, L"Table cell editor", L"表格单元格编辑器") \
    X(TableCellDropdown, L"Table cell dropdown", L"表格单元格下拉框") \
    X(ImageDetails, L"Image details", L"图像详情") \
    X(ColorAndHdr, L"Color and HDR", L"颜色和 HDR") \
    X(Exif, L"EXIF", L"EXIF") \
    X(Info, L"Info", L"信息") \
    X(Name, L"Name", L"名称") \
    X(Path, L"Path", L"路径") \
    X(Dimensions, L"Dimensions", L"尺寸") \
    X(Type, L"Type", L"类型") \
    X(FileSize, L"File size", L"文件大小") \
    X(Modified, L"Modified", L"修改时间") \
    X(ColorHdr, L"Color / HDR", L"颜色 / HDR") \
    X(Histogram, L"Histogram", L"直方图") \
    X(Unavailable, L"Unavailable", L"不可用") \
    X(ColorSummary, L"Color summary", L"颜色摘要") \
    X(AverageAbbrev, L"Avg", L"平均") \
    X(Dark, L"Dark", L"暗部") \
    X(Bright, L"Bright", L"亮部") \
    X(Luma, L"Luma", L"亮度") \
    X(ClipboardImage, L"Clipboard image", L"剪贴板图像") \
    X(Yes, L"Yes", L"是") \
    X(No, L"No", L"否") \
    X(Container, L"Container", L"容器") \
    X(SourcePixels, L"Source pixels", L"源像素") \
    X(WicFormat, L"WIC format", L"WIC 格式") \
    X(IccProfile, L"ICC profile", L"ICC 配置文件") \
    X(Primaries, L"Primaries", L"原色") \
    X(Transfer, L"Transfer", L"传递函数") \
    X(HdrMetadata, L"HDR metadata", L"HDR 元数据") \
    X(HdrSource, L"HDR source", L"HDR 源") \
    X(DisplayPath, L"Display path", L"显示路径") \
    X(SourcePreserved, L"Source preserved", L"源已保留") \
    X(DateTaken, L"Date taken", L"拍摄日期") \
    X(CameraMake, L"Camera make", L"相机厂商") \
    X(CameraModel, L"Camera model", L"相机型号") \
    X(Lens, L"Lens", L"镜头") \
    X(FocalLength, L"Focal length", L"焦距") \
    X(Aperture, L"Aperture", L"光圈") \
    X(ExposureTime, L"Exposure time", L"曝光时间") \
    X(Iso, L"ISO", L"ISO") \
    X(ExposureBias, L"Exposure bias", L"曝光补偿") \
    X(Orientation, L"Orientation", L"方向") \
    X(OrientationNormal, L"Normal", L"正常") \
    X(OrientationMirroredHorizontal, L"Mirrored horizontal", L"水平镜像") \
    X(OrientationRotated180, L"Rotated 180", L"旋转 180 度") \
    X(OrientationMirroredVertical, L"Mirrored vertical", L"垂直镜像") \
    X(OrientationMirroredHorizontalRotated270, L"Mirrored horizontal, rotated 270", L"水平镜像并旋转 270 度") \
    X(OrientationRotated90, L"Rotated 90", L"旋转 90 度") \
    X(OrientationMirroredHorizontalRotated90, L"Mirrored horizontal, rotated 90", L"水平镜像并旋转 90 度") \
    X(OrientationRotated270, L"Rotated 270", L"旋转 270 度") \
    X(DevelopmentBuild, L"Development build", L"开发构建") \
    X(AboutDescription, L"Lightweight native image viewer.", L"轻量级原生图像查看器。") \
    X(ThirdPartyNotices, L"Third-party notices", L"第三方声明") \
    X(CopiedNotices, L"Copied notices.", L"已复制声明。") \
    X(CouldNotCopyNotices, L"Could not copy notices.", L"无法复制声明。") \
    X(EditModeOn, L"Edit mode on.", L"编辑模式已开启。") \
    X(EditModeOff, L"Edit mode off.", L"编辑模式已关闭。") \
    X(CouldNotShowFileInExplorer, L"Could not show file in Explorer.", L"无法在资源管理器中显示文件。") \
    X(CouldNotCopyColor, L"Could not copy color.", L"无法复制颜色。") \
    X(CopiedSelectedPixels, L"Copied selected pixels.", L"已复制选中像素。") \
    X(CouldNotCopySelectedPixels, L"Could not copy selected pixels.", L"无法复制选中像素。") \
    X(CouldNotCaptureRegion, L"Could not capture region.", L"无法捕获区域。") \
    X(SavedSdrCopyHdrSourcePreserved, L"Saved SDR copy. HDR source preserved.", L"已保存 SDR 副本，HDR 源已保留。") \
    X(SavedImage, L"Saved image.", L"已保存图像。") \
    X(ClipboardDoesNotContainImageOrPath, L"Clipboard does not contain an image or path.", L"剪贴板不包含图像或路径。") \
    X(CouldNotPasteClipboardImage, L"Could not paste clipboard image.", L"无法粘贴剪贴板图像。") \
    X(CouldNotOpenSelectedImage, L"Could not open the selected image.", L"无法打开选中的图像。") \
    X(CouldNotReadImageFolder, L"Could not read the image folder.", L"无法读取图像文件夹。") \
    X(CouldNotShowImagePicker, L"Could not show the image picker.", L"无法显示图像选择器。") \
    X(CouldNotShowSaveDialog, L"Could not show the save dialog.", L"无法显示保存对话框。") \
    X(CouldNotSaveImage, L"Could not save the image.", L"无法保存图像。") \
    X(SavedSdrAnnotationExportHdrSourcePreserved, L"Saved SDR annotation export. HDR source preserved.", L"已保存 SDR 标注导出，HDR 源已保留。") \
    X(NoImage, L"No image", L"无图像") \
    X(ScreenshotImage, L"Screenshot image", L"截图图像") \
    X(ImagesFilter, L"Images", L"图像") \
    X(AllFilesFilter, L"All files", L"所有文件") \
    X(PngImageFilter, L"PNG image", L"PNG 图像") \
    X(DeveloperControlLab, L"Control Lab", L"控件实验室") \
    X(SampleCheckbox, L"Sample checkbox", L"示例复选框") \
    X(SampleSlider, L"Sample slider", L"示例滑块") \
    X(EditableTable, L"Editable table", L"可编辑表格") \
    X(DeveloperState, L"Developer state", L"开发者状态") \
    X(Value, L"Value", L"值") \
    X(Tool, L"Tool", L"工具") \
    X(Size, L"Size", L"大小") \
    X(Color, L"Color", L"颜色") \
    X(Background, L"Background", L"背景") \
    X(Transparent, L"Transparent", L"透明") \
    X(CropSelection, L"Crop / Selection", L"裁剪 / 选区") \
    X(DeleteSelection, L"Delete Selection", L"删除选区") \
    X(History, L"History", L"历史")

constexpr std::array kStrings{
#define IMGVIEWER_STRING_ENTRY(id, en, zh) LocalizedString{ImgViewerStringId::id, en, zh},
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

std::atomic<ImgViewerLanguage> g_language = ImgViewerLanguage::English;

} // namespace

ImgViewerLanguage CurrentImgViewerLanguage()
{
    return g_language.load(std::memory_order_relaxed);
}

void SetImgViewerLanguage(ImgViewerLanguage language)
{
    g_language.store(language, std::memory_order_relaxed);
}

const wchar_t* ImgViewerString(ImgViewerStringId id)
{
    return ImgViewerString(id, CurrentImgViewerLanguage());
}

const wchar_t* ImgViewerString(ImgViewerStringId id, ImgViewerLanguage language)
{
    const LocalizedString* entry = FindString(id);
    if (entry == nullptr) {
        return L"";
    }
    if (language == ImgViewerLanguage::SimplifiedChinese && entry->zh[0] != L'\0') {
        return entry->zh;
    }
    return entry->en;
}

const wchar_t* ImgViewerStringZh(ImgViewerStringId id)
{
    return ImgViewerString(id, ImgViewerLanguage::SimplifiedChinese);
}
