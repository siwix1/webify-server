#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <mutex>

namespace webify {

struct DesktopSession {
    std::string id;
    std::string desktop_name;
#ifdef _WIN32
    HDESK desktop_handle = nullptr;
    HANDLE process_handle = nullptr;
    DWORD process_id = 0;
#endif
    int width = 1024;
    int height = 768;
    bool active = false;
};

// Manages creation/destruction of Win32 desktops and launching apps on them.
class DesktopManager {
public:
    DesktopManager();
    ~DesktopManager();

    // Create a new desktop and launch an application on it.
    // Returns a session ID on success, empty string on failure.
    std::string create_session(const std::string& app_path,
                                const std::string& app_args = "",
                                int width = 1024,
                                int height = 768);

    // Destroy a session — close the app and remove the desktop.
    bool destroy_session(const std::string& session_id);

    // Get session info.
    const DesktopSession* get_session(const std::string& session_id) const;

    // List all active session IDs.
    std::vector<std::string> list_sessions() const;

private:
    std::string generate_id();

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<DesktopSession>> sessions_;
    int next_id_ = 0;
};

} // namespace webify
