#!/usr/bin/env python
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import json
import subprocess
import sys
import time
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parent
HELPER_SOURCE = ROOT / "winapp_ui_helper.cs"
HELPER_CACHE = ROOT / ".cache"
HELPER_EXE = HELPER_CACHE / "winapp_ui_helper.exe"
CSC_EXE = Path(r"C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe")
UIA_CLIENT = Path(r"C:\Windows\Microsoft.NET\Framework64\v4.0.30319\WPF\UIAutomationClient.dll")
UIA_TYPES = Path(r"C:\Windows\Microsoft.NET\Framework64\v4.0.30319\WPF\UIAutomationTypes.dll")
WINDOWS_BASE = Path(r"C:\Windows\Microsoft.NET\Framework64\v4.0.30319\WPF\WindowsBase.dll")

user32 = ctypes.windll.user32

EnumWindowsProc = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)

KEYUP = 0x0002
UNICODE = 0x0004
LEFTDOWN = 0x0002
LEFTUP = 0x0004
RIGHTDOWN = 0x0008
RIGHTUP = 0x0010
MIDDLEDOWN = 0x0020
MIDDLEUP = 0x0040

VK_MAP = {
    "enter": 0x0D,
    "esc": 0x1B,
    "escape": 0x1B,
    "tab": 0x09,
    "space": 0x20,
    "left": 0x25,
    "up": 0x26,
    "right": 0x27,
    "down": 0x28,
    "home": 0x24,
    "end": 0x23,
    "pgup": 0x21,
    "pgdn": 0x22,
    "delete": 0x2E,
    "backspace": 0x08,
    "ctrl": 0x11,
    "shift": 0x10,
    "alt": 0x12,
    "f1": 0x70,
    "f2": 0x71,
    "f3": 0x72,
    "f4": 0x73,
    "f5": 0x74,
    "f6": 0x75,
    "f7": 0x76,
    "f8": 0x77,
    "f9": 0x78,
    "f10": 0x79,
    "f11": 0x7A,
    "f12": 0x7B,
}


class KEYBDINPUT(ctypes.Structure):
    _fields_ = [
        ("wVk", ctypes.c_ushort),
        ("wScan", ctypes.c_ushort),
        ("dwFlags", ctypes.c_uint),
        ("time", ctypes.c_uint),
        ("dwExtraInfo", ctypes.c_void_p),
    ]


class INPUTUNION(ctypes.Union):
    _fields_ = [("ki", KEYBDINPUT)]


class INPUT(ctypes.Structure):
    _fields_ = [("type", ctypes.c_uint), ("union", INPUTUNION)]


def ensure_helper() -> Path:
    HELPER_CACHE.mkdir(exist_ok=True)
    if HELPER_EXE.exists() and HELPER_EXE.stat().st_mtime >= HELPER_SOURCE.stat().st_mtime:
        return HELPER_EXE

    command = [
        str(CSC_EXE),
        "/nologo",
        "/target:exe",
        f"/out:{HELPER_EXE}",
        f"/r:{UIA_CLIENT}",
        f"/r:{UIA_TYPES}",
        f"/r:{WINDOWS_BASE}",
        "/r:System.Drawing.dll",
        str(HELPER_SOURCE),
    ]
    subprocess.run(command, check=True)
    return HELPER_EXE


def enum_windows() -> list[dict[str, Any]]:
    windows: list[dict[str, Any]] = []

    def callback(hwnd: int, _lparam: int) -> bool:
        if not user32.IsWindowVisible(hwnd):
            return True

        title_length = user32.GetWindowTextLengthW(hwnd)
        title_buffer = ctypes.create_unicode_buffer(title_length + 1)
        user32.GetWindowTextW(hwnd, title_buffer, title_length + 1)

        class_buffer = ctypes.create_unicode_buffer(256)
        user32.GetClassNameW(hwnd, class_buffer, 256)

        rect = wintypes.RECT()
        user32.GetWindowRect(hwnd, ctypes.byref(rect))

        windows.append(
            {
                "hwnd": int(hwnd),
                "title": title_buffer.value,
                "class_name": class_buffer.value,
                "left": rect.left,
                "top": rect.top,
                "right": rect.right,
                "bottom": rect.bottom,
            }
        )
        return True

    user32.EnumWindows(EnumWindowsProc(callback), 0)
    return windows


def select_window(args: argparse.Namespace) -> dict[str, Any]:
    if args.hwnd is not None:
        return {"hwnd": args.hwnd}

    windows = enum_windows()
    matches = windows
    if args.class_name:
        matches = [window for window in matches if window["class_name"] == args.class_name]
    if args.title_contains:
        needle = args.title_contains.lower()
        matches = [window for window in matches if needle in window["title"].lower()]
    if not matches:
        raise SystemExit("No matching window found.")
    return matches[0]


def run_helper(*helper_args: str) -> None:
    helper = ensure_helper()
    subprocess.run([str(helper), *helper_args], check=True)


def resolve_point(args: argparse.Namespace) -> tuple[int, int]:
    x = int(args.x)
    y = int(args.y)
    if not args.window_relative:
        return x, y

    window = select_window(args)
    return window["left"] + x, window["top"] + y


def mouse_click(button: str, count: int) -> None:
    flags = {
        "left": (LEFTDOWN, LEFTUP),
        "right": (RIGHTDOWN, RIGHTUP),
        "middle": (MIDDLEDOWN, MIDDLEUP),
    }[button]
    for _ in range(count):
        user32.mouse_event(flags[0], 0, 0, 0, 0)
        user32.mouse_event(flags[1], 0, 0, 0, 0)
        time.sleep(0.05)


def send_vk(vk: int, key_up: bool = False) -> None:
    event = KEYBDINPUT(wVk=vk, wScan=0, dwFlags=KEYUP if key_up else 0, time=0, dwExtraInfo=None)
    packet = INPUT(type=1, union=INPUTUNION(ki=event))
    user32.SendInput(1, ctypes.byref(packet), ctypes.sizeof(packet))


def send_text(text: str) -> None:
    for char in text:
        key_down = KEYBDINPUT(wVk=0, wScan=ord(char), dwFlags=UNICODE, time=0, dwExtraInfo=None)
        key_up = KEYBDINPUT(wVk=0, wScan=ord(char), dwFlags=UNICODE | KEYUP, time=0, dwExtraInfo=None)
        packets = (INPUT * 2)(
            INPUT(type=1, union=INPUTUNION(ki=key_down)),
            INPUT(type=1, union=INPUTUNION(ki=key_up)),
        )
        user32.SendInput(2, packets, ctypes.sizeof(INPUT))


def parse_key(token: str) -> int:
    if len(token) == 1:
        return ord(token.upper())
    if token.lower() in VK_MAP:
        return VK_MAP[token.lower()]
    raise SystemExit(f"Unknown key token: {token}")


def cmd_find_window(args: argparse.Namespace) -> None:
    windows = enum_windows()
    if args.class_name:
        windows = [window for window in windows if window["class_name"] == args.class_name]
    if args.title_contains:
        needle = args.title_contains.lower()
        windows = [window for window in windows if needle in window["title"].lower()]
    print(json.dumps(windows, ensure_ascii=False, indent=2))


def cmd_tree(args: argparse.Namespace) -> None:
    window = select_window(args)
    run_helper("tree", str(window["hwnd"]))


def cmd_screenshot(args: argparse.Namespace) -> None:
    window = select_window(args)
    run_helper("screenshot", str(window["hwnd"]), args.mode, str(Path(args.out).resolve()))


def cmd_move(args: argparse.Namespace) -> None:
    x, y = resolve_point(args)
    user32.SetCursorPos(x, y)
    print(json.dumps({"x": x, "y": y}))


def cmd_click(args: argparse.Namespace) -> None:
    x, y = resolve_point(args)
    user32.SetCursorPos(x, y)
    time.sleep(0.05)
    mouse_click(args.button, args.count)
    print(json.dumps({"x": x, "y": y, "button": args.button, "count": args.count}))


def cmd_key(args: argparse.Namespace) -> None:
    keys = [parse_key(token) for token in args.keys]
    for vk in keys:
        send_vk(vk, key_up=False)
    for vk in reversed(keys):
        send_vk(vk, key_up=True)
    print(json.dumps({"keys": args.keys}))


def cmd_type(args: argparse.Namespace) -> None:
    send_text(args.text)
    print(json.dumps({"text": args.text}))


def cmd_image_info(args: argparse.Namespace) -> None:
    run_helper("image-info", str(Path(args.path).resolve()))


def cmd_image_diff(args: argparse.Namespace) -> None:
    run_helper("image-diff", str(Path(args.left).resolve()), str(Path(args.right).resolve()))


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Local WinApp UI helper for imgviewer.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    find_window = subparsers.add_parser("find-window")
    find_window.add_argument("--class-name")
    find_window.add_argument("--title-contains")
    find_window.set_defaults(func=cmd_find_window)

    for name in ("tree", "screenshot", "move", "click"):
        subparser = subparsers.add_parser(name)
        subparser.add_argument("--hwnd", type=int)
        subparser.add_argument("--class-name", default="ImgViewerWindow")
        subparser.add_argument("--title-contains")
        if name in ("move", "click"):
            subparser.add_argument("x", type=int)
            subparser.add_argument("y", type=int)
            subparser.add_argument("--window-relative", action="store_true")
        if name == "click":
            subparser.add_argument("--button", choices=("left", "right", "middle"), default="left")
            subparser.add_argument("--count", type=int, default=1)
        if name == "screenshot":
            subparser.add_argument("--mode", choices=("printwindow", "screen"), default="screen")
            subparser.add_argument("--out", required=True)

    subparsers.choices["tree"].set_defaults(func=cmd_tree)
    subparsers.choices["screenshot"].set_defaults(func=cmd_screenshot)
    subparsers.choices["move"].set_defaults(func=cmd_move)
    subparsers.choices["click"].set_defaults(func=cmd_click)

    key = subparsers.add_parser("key")
    key.add_argument("keys", nargs="+")
    key.set_defaults(func=cmd_key)

    text = subparsers.add_parser("type")
    text.add_argument("text")
    text.set_defaults(func=cmd_type)

    image_info = subparsers.add_parser("image-info")
    image_info.add_argument("path")
    image_info.set_defaults(func=cmd_image_info)

    image_diff = subparsers.add_parser("image-diff")
    image_diff.add_argument("left")
    image_diff.add_argument("right")
    image_diff.set_defaults(func=cmd_image_diff)

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    args.func(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
