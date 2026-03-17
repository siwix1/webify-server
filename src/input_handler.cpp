#include "input_handler.h"

#include <cstdio>

namespace webify {

InputHandler::InputHandler() = default;

InputHandler::~InputHandler() {
    detach();
}

bool InputHandler::attach(const std::string& desktop_name) {
    attached_ = true;
    fprintf(stdout, "InputHandler: ready (target HWND mode)\n");
    return true;
}

void InputHandler::detach() {
    attached_ = false;
    target_hwnd_ = nullptr;
}

void InputHandler::mouse_move(int x, int y) {
#ifdef _WIN32
    if (!target_hwnd_) return;
    last_mouse_x_ = x;
    last_mouse_y_ = y;

    // Convert client coords to screen coords, then use SendInput
    POINT pt = {x, y};
    ClientToScreen(target_hwnd_, &pt);

    // SendInput expects normalized absolute coordinates (0-65535)
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    int abs_x = (int)((pt.x * 65535.0) / screen_w);
    int abs_y = (int)((pt.y * 65535.0) / screen_h);

    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dx = abs_x;
    input.mi.dy = abs_y;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    SendInput(1, &input, sizeof(INPUT));
#endif
}

void InputHandler::mouse_button(int button, bool down) {
#ifdef _WIN32
    if (!target_hwnd_) return;

    INPUT input = {};
    input.type = INPUT_MOUSE;

    switch (button) {
        case 0: // left
            input.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
            break;
        case 1: // right (browser button 2)
        case 2: // right
            input.mi.dwFlags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
            break;
        default:
            return;
    }

    SendInput(1, &input, sizeof(INPUT));
#endif
}

void InputHandler::mouse_scroll(int delta) {
#ifdef _WIN32
    if (!target_hwnd_) return;

    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = delta;
    SendInput(1, &input, sizeof(INPUT));
#endif
}

void InputHandler::key_event(uint16_t vk_code, bool down) {
#ifdef _WIN32
    if (!target_hwnd_) return;

    // Track shift state locally
    if (vk_code == VK_SHIFT || vk_code == VK_LSHIFT || vk_code == VK_RSHIFT) {
        shift_held_ = down;
    }

    UINT scan = MapVirtualKey(vk_code, MAPVK_VK_TO_VSC);

    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk_code;
    input.ki.wScan = (WORD)scan;
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;

    // Extended key flag for navigation keys
    if (vk_code >= VK_PRIOR && vk_code <= VK_DELETE) {
        input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }

    SendInput(1, &input, sizeof(INPUT));
#endif
}

} // namespace webify
