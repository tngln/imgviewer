#include "ui.events.hpp"

#include "win32.util.hpp"

UiModifiers UiModifiers::Current()
{
    return UiModifiers{
        .ctrl = util::IsKeyDown(VK_CONTROL),
        .shift = util::IsKeyDown(VK_SHIFT),
        .alt = util::IsKeyDown(VK_MENU),
    };
}
