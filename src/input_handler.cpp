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

#ifdef _WIN32
// Find the deepest child window at the given client coordinates of target_hwnd_
// and return it along with coordinates mapped to that child's client area.
HWND InputHandler::child_at(int x, int y, POINT& out_pt) {
    out_pt = {x, y};
    if (!target_hwnd_) return nullptr;

    // Convert to screen coords
    POINT screen_pt = {x, y};
    ClientToScreen(target_hwnd_, &screen_pt);

    // Use RealChildWindowFromPoint recursively to find the deepest child
    HWND current = target_hwnd_;
    while (true) {
        POINT client_pt = screen_pt;
        ScreenToClient(current, &client_pt);
        HWND child = RealChildWindowFromPoint(current, client_pt);
        if (!child || child == current) {
            out_pt = client_pt;
            return current;
        }
        current = child;
    }
}
#endif

void InputHandler::mouse_move(int x, int y) {
#ifdef _WIN32
    if (!target_hwnd_) return;
    last_mouse_x_ = x;
    last_mouse_y_ = y;

    POINT pt;
    HWND target = child_at(x, y, pt);
    if (!target) return;

    PostMessage(target, WM_MOUSEMOVE, 0, MAKELPARAM(pt.x, pt.y));
#endif
}

void InputHandler::mouse_button(int button, bool down) {
#ifdef _WIN32
    if (!target_hwnd_) return;
    UINT msg = 0;
    WPARAM wParam = 0;

    switch (button) {
        case 0: // left
            msg = down ? WM_LBUTTONDOWN : WM_LBUTTONUP;
            wParam = down ? MK_LBUTTON : 0;
            break;
        case 1: // right (browser button 2)
        case 2: // right
            msg = down ? WM_RBUTTONDOWN : WM_RBUTTONUP;
            wParam = down ? MK_RBUTTON : 0;
            break;
        default:
            return;
    }

    POINT pt;
    HWND target = child_at(last_mouse_x_, last_mouse_y_, pt);
    if (!target) return;

    // Track which child was last clicked for keyboard targeting
    if (down && button == 0) {
        focused_child_ = target;
    }

    PostMessage(target, msg, wParam, MAKELPARAM(pt.x, pt.y));
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

    // Track shift state locally
    if (vk_code == VK_SHIFT || vk_code == VK_LSHIFT || vk_code == VK_RSHIFT) {
        shift_held_ = down;
    }

    UINT scan = MapVirtualKey(vk_code, MAPVK_VK_TO_VSC);
    LPARAM lParam = 1 | (scan << 16);

    // Extended key flag
    if (vk_code >= VK_PRIOR && vk_code <= VK_DELETE) {
        lParam |= (1 << 24);
    }

    if (!down) {
        lParam |= (1 << 30) | (1 << 31);
    }

    // Send to last clicked child, or main window
    HWND target = focused_child_ ? focused_child_ : target_hwnd_;

    PostMessage(target, down ? WM_KEYDOWN : WM_KEYUP, vk_code, lParam);

    // Send WM_CHAR for printable characters
    if (down && vk_code >= 0x20 && vk_code <= 0x7E) {
        char ch = (char)vk_code;
        if (!shift_held_ && ch >= 'A' && ch <= 'Z') {
            ch = ch - 'A' + 'a';
        }
        PostMessage(target, WM_CHAR, ch, lParam);
    }
#endif
}

} // namespace webify
