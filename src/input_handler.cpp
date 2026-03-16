#include "input_handler.h"

#include <cstdio>

namespace webify {

InputHandler::InputHandler() = default;

InputHandler::~InputHandler() {
    detach();
}

bool InputHandler::attach(const std::string& desktop_name) {
    // Legacy — just mark as attached. Real input goes via PostMessage to target_hwnd_.
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
    LPARAM lParam = MAKELPARAM(x, y);
    PostMessage(target_hwnd_, WM_MOUSEMOVE, 0, lParam);
#endif
}

void InputHandler::mouse_button(int button, bool down) {
#ifdef _WIN32
    if (!target_hwnd_) return;
    LPARAM lParam = MAKELPARAM(last_mouse_x_, last_mouse_y_);
    UINT msg = 0;
    WPARAM wParam = 0;

    switch (button) {
        case 0: // left
            msg = down ? WM_LBUTTONDOWN : WM_LBUTTONUP;
            wParam = down ? MK_LBUTTON : 0;
            break;
        case 1: // right  (browser button 2)
        case 2: // right
            msg = down ? WM_RBUTTONDOWN : WM_RBUTTONUP;
            wParam = down ? MK_RBUTTON : 0;
            break;
        default:
            return;
    }

    PostMessage(target_hwnd_, msg, wParam, lParam);
#endif
}

void InputHandler::mouse_scroll(int delta) {
#ifdef _WIN32
    if (!target_hwnd_) return;
    WPARAM wParam = MAKEWPARAM(0, (SHORT)delta);
    LPARAM lParam = MAKELPARAM(last_mouse_x_, last_mouse_y_);
    PostMessage(target_hwnd_, WM_MOUSEWHEEL, wParam, lParam);
#endif
}

void InputHandler::key_event(uint16_t vk_code, bool down) {
#ifdef _WIN32
    if (!target_hwnd_) return;

    UINT scan = MapVirtualKey(vk_code, MAPVK_VK_TO_VSC);
    LPARAM lParam = 1 | (scan << 16);

    // Extended key flag
    if (vk_code >= VK_PRIOR && vk_code <= VK_DELETE) {
        lParam |= (1 << 24);
    }

    if (!down) {
        lParam |= (1 << 30) | (1 << 31); // previous state + transition
    }

    PostMessage(target_hwnd_, down ? WM_KEYDOWN : WM_KEYUP, vk_code, lParam);

    // Also send WM_CHAR for printable characters on keydown
    if (down && vk_code >= 0x20 && vk_code <= 0x7E) {
        // For letters, check if shift is held
        SHORT shift_state = GetAsyncKeyState(VK_SHIFT);
        char ch = (char)vk_code;
        if (!(shift_state & 0x8000) && ch >= 'A' && ch <= 'Z') {
            ch = ch - 'A' + 'a'; // lowercase
        }
        PostMessage(target_hwnd_, WM_CHAR, ch, lParam);
    }
#endif
}

} // namespace webify
