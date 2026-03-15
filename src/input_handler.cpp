#include "input_handler.h"

#include <cstdio>

namespace webify {

InputHandler::InputHandler() = default;

InputHandler::~InputHandler() {
    detach();
}

bool InputHandler::attach(const std::string& desktop_name) {
    if (attached_) detach();

#ifdef _WIN32
    desktop_ = OpenDesktopA(desktop_name.c_str(), 0, FALSE, GENERIC_ALL);
    if (!desktop_) {
        fprintf(stderr, "InputHandler: OpenDesktop failed: %lu\n", GetLastError());
        return false;
    }

    if (!SetThreadDesktop(desktop_)) {
        fprintf(stderr, "InputHandler: SetThreadDesktop failed: %lu\n", GetLastError());
        CloseDesktop(desktop_);
        desktop_ = nullptr;
        return false;
    }
#endif

    attached_ = true;
    fprintf(stdout, "InputHandler: attached to desktop '%s'\n", desktop_name.c_str());
    return true;
}

void InputHandler::detach() {
    if (!attached_) return;

#ifdef _WIN32
    if (desktop_) {
        CloseDesktop(desktop_);
        desktop_ = nullptr;
    }
#endif

    attached_ = false;
}

void InputHandler::mouse_move(int x, int y) {
    if (!attached_) return;

#ifdef _WIN32
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dx = x;
    input.mi.dy = y;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;

    // Convert to absolute coordinates (0-65535 range)
    // The caller should provide pixel coords; we normalize here.
    // For now, assume the desktop is the primary monitor size.
    // TODO: get actual desktop resolution
    input.mi.dx = static_cast<LONG>((x * 65535) / 1024);
    input.mi.dy = static_cast<LONG>((y * 65535) / 768);

    SendInput(1, &input, sizeof(INPUT));
#endif
}

void InputHandler::mouse_button(int button, bool down) {
    if (!attached_) return;

#ifdef _WIN32
    INPUT input = {};
    input.type = INPUT_MOUSE;

    switch (button) {
        case 0: // left
            input.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
            break;
        case 1: // right
            input.mi.dwFlags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
            break;
        case 2: // middle
            input.mi.dwFlags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
            break;
        default:
            return;
    }

    SendInput(1, &input, sizeof(INPUT));
#endif
}

void InputHandler::mouse_scroll(int delta) {
    if (!attached_) return;

#ifdef _WIN32
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = static_cast<DWORD>(delta);

    SendInput(1, &input, sizeof(INPUT));
#endif
}

void InputHandler::key_event(uint16_t vk_code, bool down) {
    if (!attached_) return;

#ifdef _WIN32
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk_code;
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;

    // Extended keys (arrows, insert, delete, etc.)
    if (vk_code >= VK_PRIOR && vk_code <= VK_DELETE) {
        input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }

    SendInput(1, &input, sizeof(INPUT));
#endif
}

} // namespace webify
