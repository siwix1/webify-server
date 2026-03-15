#include "desktop_manager.h"

#include <sstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <userenv.h>
#pragma comment(lib, "userenv.lib")
#endif

namespace webify {

DesktopManager::DesktopManager() = default;

DesktopManager::~DesktopManager() {
    // Clean up all sessions
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, session] : sessions_) {
#ifdef _WIN32
        if (session->process_handle) {
            TerminateProcess(session->process_handle, 0);
            CloseHandle(session->process_handle);
        }
        if (session->desktop_handle) {
            CloseDesktop(session->desktop_handle);
        }
#endif
    }
    sessions_.clear();
}

std::string DesktopManager::generate_id() {
    std::ostringstream oss;
    oss << "session_" << next_id_++;
    return oss.str();
}

std::string DesktopManager::create_session(const std::string& app_path,
                                            const std::string& app_args,
                                            int width, int height) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto session = std::make_unique<DesktopSession>();
    session->id = generate_id();
    session->width = width;
    session->height = height;

#ifdef _WIN32
    // Create a unique desktop name
    session->desktop_name = "webify_" + session->id;

    // CreateDesktopA creates a new desktop within the current window station.
    // The desktop is initially empty — no explorer shell, no taskbar.
    // This is exactly what we want: a clean canvas for the app.
    HDESK hDesktop = CreateDesktopA(
        session->desktop_name.c_str(),
        nullptr,                        // reserved
        nullptr,                        // reserved
        0,                              // flags
        GENERIC_ALL,                    // access
        nullptr                         // security attributes
    );

    if (!hDesktop) {
        DWORD err = GetLastError();
        fprintf(stderr, "CreateDesktop failed: %lu\n", err);
        return "";
    }

    session->desktop_handle = hDesktop;

    // Build command line
    std::string cmd_line = app_path;
    if (!app_args.empty()) {
        cmd_line += " " + app_args;
    }

    // Launch the application on the new desktop.
    // STARTUPINFOA.lpDesktop tells Windows which desktop to use.
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.lpDesktop = const_cast<char*>(session->desktop_name.c_str());

    PROCESS_INFORMATION pi = {};

    BOOL ok = CreateProcessA(
        nullptr,                              // application name (use cmd_line)
        const_cast<char*>(cmd_line.c_str()),  // command line
        nullptr,                              // process security
        nullptr,                              // thread security
        FALSE,                                // inherit handles
        0,                                    // creation flags
        nullptr,                              // environment (inherit)
        nullptr,                              // working directory (inherit)
        &si,
        &pi
    );

    if (!ok) {
        DWORD err = GetLastError();
        fprintf(stderr, "CreateProcess failed: %lu (app: %s)\n", err, app_path.c_str());
        CloseDesktop(hDesktop);
        return "";
    }

    session->process_handle = pi.hProcess;
    session->process_id = pi.dwProcessId;
    session->active = true;

    // Close the thread handle — we only need the process handle.
    CloseHandle(pi.hThread);

    fprintf(stdout, "Session %s created: desktop=%s pid=%lu\n",
            session->id.c_str(),
            session->desktop_name.c_str(),
            session->process_id);
#else
    // Non-Windows stub for compilation
    session->desktop_name = "webify_" + session->id;
    session->active = true;
    fprintf(stdout, "Session %s created (stub, non-Windows)\n", session->id.c_str());
#endif

    std::string id = session->id;
    sessions_[id] = std::move(session);
    return id;
}

bool DesktopManager::destroy_session(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) return false;

    auto& session = it->second;

#ifdef _WIN32
    // Terminate the process
    if (session->process_handle) {
        TerminateProcess(session->process_handle, 0);
        WaitForSingleObject(session->process_handle, 3000);
        CloseHandle(session->process_handle);
        session->process_handle = nullptr;
    }

    // Close the desktop
    if (session->desktop_handle) {
        CloseDesktop(session->desktop_handle);
        session->desktop_handle = nullptr;
    }
#endif

    fprintf(stdout, "Session %s destroyed\n", session_id.c_str());
    sessions_.erase(it);
    return true;
}

const DesktopSession* DesktopManager::get_session(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) return nullptr;
    return it->second.get();
}

std::vector<std::string> DesktopManager::list_sessions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> ids;
    ids.reserve(sessions_.size());
    for (const auto& [id, _] : sessions_) {
        ids.push_back(id);
    }
    return ids;
}

} // namespace webify
