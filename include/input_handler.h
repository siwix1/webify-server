#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include <string>
#include <cstdint>

namespace webify {

// Sends mouse and keyboard input to a specific window via PostMessage.
// Does not require SetThreadDesktop or SetForegroundWindow.
class InputHandler {
public:
    InputHandler();
    ~InputHandler();

    // Attach to a desktop by name (legacy, kept for API compat)
    bool attach(const std::string& desktop_name);
    void detach();

    // Set target window — all input goes here via PostMessage
    void set_target(HWND hwnd) { target_hwnd_ = hwnd; }

    // Mouse events (x, y in client coordinates of the target window)
    void mouse_move(int x, int y);
    void mouse_button(int button, bool down);
    void mouse_scroll(int delta);

    // Keyboard events
    void key_event(uint16_t vk_code, bool down);

private:
#ifdef _WIN32
    HWND target_hwnd_ = nullptr;
#endif
    bool attached_ = false;
    int last_mouse_x_ = 0;
    int last_mouse_y_ = 0;
};

} // namespace webify
