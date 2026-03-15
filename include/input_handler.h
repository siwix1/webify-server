#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

#include <string>
#include <cstdint>

namespace webify {

// Injects mouse and keyboard input into a specific desktop.
class InputHandler {
public:
    InputHandler();
    ~InputHandler();

    // Attach to a desktop by name — input will be sent there.
    bool attach(const std::string& desktop_name);
    void detach();

    // Mouse events
    void mouse_move(int x, int y);
    void mouse_button(int button, bool down);  // button: 0=left, 1=right, 2=middle
    void mouse_scroll(int delta);

    // Keyboard events
    void key_event(uint16_t vk_code, bool down);

private:
#ifdef _WIN32
    HDESK desktop_ = nullptr;
#endif
    bool attached_ = false;
};

} // namespace webify
