using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows.Automation;

internal static class Program
{
    [StructLayout(LayoutKind.Sequential)]
    private struct RECT
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll")]
    private static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    private static extern bool PrintWindow(IntPtr hwnd, IntPtr hdcBlt, int nFlags);

    [DllImport("user32.dll")]
    private static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    private static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    private static int Main(string[] args)
    {
        if (args.Length == 0)
        {
            Console.Error.WriteLine("Missing command.");
            return 1;
        }

        switch (args[0])
        {
        case "tree":
            return DumpTree(new IntPtr(long.Parse(args[1], CultureInfo.InvariantCulture)));
        case "screenshot":
            return SaveScreenshot(
                new IntPtr(long.Parse(args[1], CultureInfo.InvariantCulture)),
                args[2],
                args[3]);
        case "image-info":
            return PrintImageInfo(args[1]);
        case "image-diff":
            return PrintImageDiff(args[1], args[2]);
        default:
            Console.Error.WriteLine("Unknown command: " + args[0]);
            return 1;
        }
    }

    private static int DumpTree(IntPtr hwnd)
    {
        AutomationElement root = AutomationElement.FromHandle(hwnd);
        Console.WriteLine(SerializeElement(root));
        return 0;
    }

    private static string SerializeElement(AutomationElement element)
    {
        StringBuilder builder = new StringBuilder();
        AppendElement(builder, element);
        return builder.ToString();
    }

    private static void AppendElement(StringBuilder builder, AutomationElement element)
    {
        builder.Append("{");
        AppendProperty(builder, "name", element.Current.Name);
        builder.Append(",");
        AppendProperty(builder, "automation_id", element.Current.AutomationId);
        builder.Append(",");
        AppendProperty(builder, "class_name", element.Current.ClassName);
        builder.Append(",");
        AppendProperty(builder, "control_type", element.Current.ControlType.ProgrammaticName);
        builder.Append(",");
        builder.Append("\"bounds\":[");
        builder.Append(element.Current.BoundingRectangle.Left.ToString(CultureInfo.InvariantCulture));
        builder.Append(",");
        builder.Append(element.Current.BoundingRectangle.Top.ToString(CultureInfo.InvariantCulture));
        builder.Append(",");
        builder.Append(element.Current.BoundingRectangle.Width.ToString(CultureInfo.InvariantCulture));
        builder.Append(",");
        builder.Append(element.Current.BoundingRectangle.Height.ToString(CultureInfo.InvariantCulture));
        builder.Append("],");
        builder.Append("\"children\":[");

        bool first = true;
        TreeWalker walker = TreeWalker.ControlViewWalker;
        for (AutomationElement child = walker.GetFirstChild(element); child != null; child = walker.GetNextSibling(child))
        {
            if (!first)
            {
                builder.Append(",");
            }
            first = false;
            AppendElement(builder, child);
        }

        builder.Append("]}");
    }

    private static void AppendProperty(StringBuilder builder, string key, string value)
    {
        builder.Append("\"");
        builder.Append(key);
        builder.Append("\":");
        builder.Append("\"");
        builder.Append(Escape(value ?? string.Empty));
        builder.Append("\"");
    }

    private static string Escape(string value)
    {
        return value.Replace("\\", "\\\\").Replace("\"", "\\\"");
    }

    private static int SaveScreenshot(IntPtr hwnd, string mode, string outputPath)
    {
        RECT rect;
        if (!GetWindowRect(hwnd, out rect))
        {
            Console.Error.WriteLine("GetWindowRect failed.");
            return 1;
        }

        int width = Math.Max(1, rect.Right - rect.Left);
        int height = Math.Max(1, rect.Bottom - rect.Top);
        string directory = Path.GetDirectoryName(outputPath);
        if (!string.IsNullOrEmpty(directory))
        {
            Directory.CreateDirectory(directory);
        }

        using (Bitmap bitmap = new Bitmap(width, height))
        {
            if (string.Equals(mode, "printwindow", StringComparison.OrdinalIgnoreCase))
            {
                using (Graphics graphics = Graphics.FromImage(bitmap))
                {
                    IntPtr hdc = graphics.GetHdc();
                    try
                    {
                        if (!PrintWindow(hwnd, hdc, 0))
                        {
                            Console.Error.WriteLine("PrintWindow failed.");
                            return 1;
                        }
                    }
                    finally
                    {
                        graphics.ReleaseHdc(hdc);
                    }
                }
            }
            else
            {
                ShowWindow(hwnd, 5);
                SetForegroundWindow(hwnd);
                Thread.Sleep(250);
                using (Graphics graphics = Graphics.FromImage(bitmap))
                {
                    graphics.CopyFromScreen(rect.Left, rect.Top, 0, 0, new Size(width, height));
                }
            }

            bitmap.Save(outputPath, ImageFormat.Png);
        }

        Console.WriteLine(outputPath);
        return 0;
    }

    private static int PrintImageInfo(string path)
    {
        using (Image image = Image.FromFile(path))
        {
            Console.WriteLine(
                string.Format(
                    CultureInfo.InvariantCulture,
                    "{{\"path\":\"{0}\",\"width\":{1},\"height\":{2},\"raw_format\":\"{3}\"}}",
                    Escape(path),
                    image.Width,
                    image.Height,
                    image.RawFormat.Guid));
        }

        return 0;
    }

    private static int PrintImageDiff(string leftPath, string rightPath)
    {
        using (Bitmap left = new Bitmap(leftPath))
        using (Bitmap right = new Bitmap(rightPath))
        {
            int width = Math.Min(left.Width, right.Width);
            int height = Math.Min(left.Height, right.Height);
            long differentPixels = 0;
            int maxChannelDelta = 0;

            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    Color a = left.GetPixel(x, y);
                    Color b = right.GetPixel(x, y);
                    int delta = Math.Max(
                        Math.Max(Math.Abs(a.R - b.R), Math.Abs(a.G - b.G)),
                        Math.Max(Math.Abs(a.B - b.B), Math.Abs(a.A - b.A)));
                    if (delta > 0)
                    {
                        ++differentPixels;
                        maxChannelDelta = Math.Max(maxChannelDelta, delta);
                    }
                }
            }

            Console.WriteLine(
                string.Format(
                    CultureInfo.InvariantCulture,
                    "{{\"left\":\"{0}\",\"right\":\"{1}\",\"same_size\":{2},\"width\":{3},\"height\":{4},\"different_pixels\":{5},\"max_channel_delta\":{6}}}",
                    Escape(leftPath),
                    Escape(rightPath),
                    left.Width == right.Width && left.Height == right.Height ? "true" : "false",
                    width,
                    height,
                    differentPixels,
                    maxChannelDelta));
        }

        return 0;
    }
}
