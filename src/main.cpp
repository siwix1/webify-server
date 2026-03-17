#include "screen_capture.h"
#include "vdd_capture.h"
#include "input_handler.h"
#include "ws_server.h"
#include "jpeg_encoder.h"
#include "client_html.h"

#include <cstdio>
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>
#include <string>
#include <mutex>
#include <unordered_map>
#include <memory>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

static std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running = false;
}

// Map browser keyCode to Windows VK code
static uint16_t browser_keycode_to_vk(int keyCode, const std::string& code) {
    if (code == "Space") return 0x20;
    if (code == "Enter" || code == "NumpadEnter") return 0x0D;
    if (code == "Backspace") return 0x08;
    if (code == "Tab") return 0x09;
    if (code == "Escape") return 0x1B;
    if (code == "Delete") return 0x2E;
    if (code == "Insert") return 0x2D;
    if (code == "Home") return 0x24;
    if (code == "End") return 0x23;
    if (code == "PageUp") return 0x21;
    if (code == "PageDown") return 0x22;
    if (code == "ArrowUp") return 0x26;
    if (code == "ArrowDown") return 0x28;
    if (code == "ArrowLeft") return 0x25;
    if (code == "ArrowRight") return 0x27;
    if (code == "ShiftLeft" || code == "ShiftRight") return 0x10;
    if (code == "ControlLeft" || code == "ControlRight") return 0x11;
    if (code == "AltLeft" || code == "AltRight") return 0x12;
    if (code == "CapsLock") return 0x14;
    if (keyCode >= 0 && keyCode <= 255) return (uint16_t)keyCode;
    return 0;
}

// Simple JSON value extraction
static std::string json_get_string(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    pos++;
    while (pos < json.size() && json[pos] == ' ') pos++;
    if (pos < json.size() && json[pos] == '"') {
        pos++;
        auto end = json.find('"', pos);
        if (end != std::string::npos) return json.substr(pos, end - pos);
    }
    return "";
}

static int json_get_int(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return 0;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return 0;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '"')) pos++;
    try { return std::stoi(json.substr(pos)); } catch (...) { return 0; }
}

#include <set>

#ifdef _WIN32
// Find the real PID for an app (handles ARM64 emulation re-launch).
// `claimed_pids` contains PIDs already assigned to other sessions — skip them.
static DWORD find_real_pid(DWORD initial_pid, const std::string& app_path,
                           const std::set<DWORD>& claimed_pids) {
    // First check if the initial PID has windows and isn't claimed
    if (claimed_pids.find(initial_pid) == claimed_pids.end()) {
        struct FindData { DWORD pid; int count; };
        FindData fd = { initial_pid, 0 };
        EnumWindows([](HWND hwnd, LPARAM param) -> BOOL {
            auto* f = (FindData*)param;
            DWORD wnd_pid = 0;
            GetWindowThreadProcessId(hwnd, &wnd_pid);
            if (wnd_pid == f->pid) {
                RECT r;
                GetWindowRect(hwnd, &r);
                if ((r.right - r.left) > 0 && (r.bottom - r.top) > 0)
                    f->count++;
            }
            return TRUE;
        }, (LPARAM)&fd);

        if (fd.count > 0) return initial_pid;
    }

    // Scan by exe name, skipping claimed PIDs
    std::string exe_name = app_path;
    auto slash = exe_name.find_last_of("\\/");
    if (slash != std::string::npos) exe_name = exe_name.substr(slash + 1);
    for (auto& c : exe_name) c = (char)tolower(c);

    struct ScanData { std::string exe; DWORD found_pid; const std::set<DWORD>* claimed; };
    ScanData sd = { exe_name, 0, &claimed_pids };
    EnumWindows([](HWND hwnd, LPARAM param) -> BOOL {
        auto* s = (ScanData*)param;
        DWORD wnd_pid = 0;
        GetWindowThreadProcessId(hwnd, &wnd_pid);

        // Skip PIDs already claimed by other sessions
        if (s->claimed->find(wnd_pid) != s->claimed->end()) return TRUE;

        HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, wnd_pid);
        if (proc) {
            char path[MAX_PATH] = {};
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageNameA(proc, 0, path, &size)) {
                std::string p(path);
                auto sl = p.find_last_of("\\/");
                if (sl != std::string::npos) p = p.substr(sl + 1);
                for (auto& c : p) c = (char)tolower(c);
                if (p == s->exe) {
                    s->found_pid = wnd_pid;
                    CloseHandle(proc);
                    return FALSE;
                }
            }
            CloseHandle(proc);
        }
        return TRUE;
    }, (LPARAM)&sd);

    return sd.found_pid ? sd.found_pid : initial_pid;
}

// Find the main HWND for a PID
static HWND find_main_window(DWORD pid) {
    struct Data { DWORD pid; HWND hwnd; };
    Data d = { pid, nullptr };
    EnumWindows([](HWND hwnd, LPARAM param) -> BOOL {
        auto* data = (Data*)param;
        DWORD wnd_pid = 0;
        GetWindowThreadProcessId(hwnd, &wnd_pid);
        if (wnd_pid == data->pid && IsWindowVisible(hwnd)) {
            RECT r;
            GetWindowRect(hwnd, &r);
            if ((r.right - r.left) > 0 && (r.bottom - r.top) > 0) {
                data->hwnd = hwnd;
                return FALSE;
            }
        }
        return TRUE;
    }, (LPARAM)&d);
    return d.hwnd;
}

// Find the first Edit child control in a window (for keyboard input)
static HWND find_edit_child(HWND parent) {
    if (!parent) return nullptr;
    struct Data { HWND found; };
    Data d = { nullptr };
    EnumChildWindows(parent, [](HWND hwnd, LPARAM param) -> BOOL {
        auto* data = (Data*)param;
        char cls[64] = {};
        GetClassNameA(hwnd, cls, sizeof(cls));
        if (_stricmp(cls, "Edit") == 0 || _stricmp(cls, "RichEdit20W") == 0) {
            data->found = hwnd;
            return FALSE;
        }
        return TRUE;
    }, (LPARAM)&d);
    return d.found;
}
#endif

// Per-client session: owns an app process, capture, and input handler
struct ClientSession {
    int client_id = 0;
    int monitor_index = -1;  // virtual monitor index when using VDD
    DWORD app_pid = 0;
    HANDLE app_process = nullptr;
    HWND app_hwnd = nullptr;
    webify::ScreenCapture capture;      // PrintWindow mode
    webify::VddCapture vdd_capture;     // VDD shared memory mode
    webify::InputHandler input;
    std::thread capture_thread;
    std::mutex frame_mutex;
    std::vector<uint8_t> latest_jpeg;
    std::atomic<bool> running{false};
};

int main(int argc, char* argv[]) {
    fprintf(stdout, "=== webify-server (multi-session) ===\n");
    fprintf(stdout, "Desktop streaming server for legacy Win32 applications\n");
    fprintf(stdout, "Each browser tab gets its own app instance\n\n");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Parse command line
    std::string app_path = "notepad.exe";
    int width = 1024;
    int height = 768;
    int fps = 10;
    uint16_t port = 8080;
    int jpeg_quality = 50;
    bool use_vdd = false;
    int max_monitors = 4;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--app" && i + 1 < argc) app_path = argv[++i];
        else if (arg == "--width" && i + 1 < argc) width = std::stoi(argv[++i]);
        else if (arg == "--height" && i + 1 < argc) height = std::stoi(argv[++i]);
        else if (arg == "--fps" && i + 1 < argc) fps = std::stoi(argv[++i]);
        else if (arg == "--port" && i + 1 < argc) port = (uint16_t)std::stoi(argv[++i]);
        else if (arg == "--quality" && i + 1 < argc) jpeg_quality = std::stoi(argv[++i]);
        else if (arg == "--vdd") use_vdd = true;
        else if (arg == "--monitors" && i + 1 < argc) max_monitors = std::stoi(argv[++i]);
        else if (arg == "--help") {
            fprintf(stdout, "Usage: webify-server [options]\n");
            fprintf(stdout, "  --app <path>       Application to launch per client (default: notepad.exe)\n");
            fprintf(stdout, "  --width <px>       Desktop width (default: 1024)\n");
            fprintf(stdout, "  --height <px>      Desktop height (default: 768)\n");
            fprintf(stdout, "  --fps <n>          Capture framerate (default: 10)\n");
            fprintf(stdout, "  --port <n>         HTTP/WebSocket port (default: 8080)\n");
            fprintf(stdout, "  --quality <1-100>  JPEG quality (default: 50)\n");
            fprintf(stdout, "  --vdd              Use virtual display driver for capture\n");
            fprintf(stdout, "  --monitors <n>     Max virtual monitors (default: 4)\n");
            return 0;
        }
    }

#ifdef _WIN32
    // Session map: client_id -> session
    std::mutex sessions_mutex;
    std::unordered_map<int, std::unique_ptr<ClientSession>> sessions;

    // Track which virtual monitors are in use (for VDD mode)
    std::vector<bool> monitor_in_use(max_monitors, false);

    if (use_vdd) {
        fprintf(stdout, "VDD mode: using virtual display driver with %d monitors\n", max_monitors);
    }

    webify::WsServer ws;
    ws.set_html(CLIENT_HTML);

    ws.set_on_connect([&](int client_id) {
        fprintf(stdout, "Client %d connected — launching %s\n", client_id, app_path.c_str());

        auto session = std::make_unique<ClientSession>();
        session->client_id = client_id;

        // In VDD mode, assign a virtual monitor
        int assigned_monitor = -1;
        if (use_vdd) {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            for (int i = 0; i < max_monitors; i++) {
                if (!monitor_in_use[i]) {
                    monitor_in_use[i] = true;
                    assigned_monitor = i;
                    break;
                }
            }
            if (assigned_monitor < 0) {
                fprintf(stderr, "Client %d: no virtual monitors available\n", client_id);
                return;
            }
        }
        session->monitor_index = assigned_monitor;

        // Launch a new app instance for this client
        static int spawn_x = 0;
        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USEPOSITION | STARTF_USESIZE;
        si.dwX = spawn_x;
        si.dwY = 0;
        si.dwXSize = width;
        si.dwYSize = height;
        spawn_x += 50;  // Stagger windows slightly
        PROCESS_INFORMATION pi = {};

        // Make a mutable copy of app_path for CreateProcessA
        std::string cmd = app_path;
        if (!CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()),
                            nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            fprintf(stderr, "Client %d: CreateProcess failed: %lu\n", client_id, GetLastError());
            if (assigned_monitor >= 0) {
                std::lock_guard<std::mutex> lock(sessions_mutex);
                monitor_in_use[assigned_monitor] = false;
            }
            return;
        }

        session->app_process = pi.hProcess;
        session->app_pid = pi.dwProcessId;
        CloseHandle(pi.hThread);
        fprintf(stdout, "Client %d: app PID %lu (monitor %d)\n",
                client_id, (unsigned long)session->app_pid, assigned_monitor);

        // Wait for window to appear
        Sleep(2000);

        // Build set of PIDs already used by other sessions
        std::set<DWORD> claimed_pids;
        {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            for (auto& [id, s] : sessions) {
                claimed_pids.insert(s->app_pid);
            }
        }

        // Resolve real PID (ARM64 emulation may re-launch)
        session->app_pid = find_real_pid(session->app_pid, app_path, claimed_pids);
        fprintf(stdout, "Client %d: resolved PID %lu\n", client_id, (unsigned long)session->app_pid);

        // Find main window and edit child for input targeting
        session->app_hwnd = find_main_window(session->app_pid);
        HWND edit_hwnd = find_edit_child(session->app_hwnd);
        fprintf(stdout, "Client %d: HWND %p, edit child %p\n",
                client_id, (void*)session->app_hwnd, (void*)edit_hwnd);

        // Start capture
        if (use_vdd && assigned_monitor >= 0) {
            // VDD mode: read frames from shared memory
            if (!session->vdd_capture.start(assigned_monitor, width, height)) {
                fprintf(stderr, "Client %d: VDD capture start failed, falling back to PrintWindow\n", client_id);
                // Fall back to PrintWindow
                if (!session->capture.start(session->app_pid, width, height, fps)) {
                    fprintf(stderr, "Client %d: capture start failed\n", client_id);
                    TerminateProcess(session->app_process, 0);
                    CloseHandle(session->app_process);
                    std::lock_guard<std::mutex> lock(sessions_mutex);
                    monitor_in_use[assigned_monitor] = false;
                    return;
                }
            }
        } else {
            // PrintWindow mode
            if (!session->capture.start(session->app_pid, width, height, fps)) {
                fprintf(stderr, "Client %d: capture start failed\n", client_id);
                TerminateProcess(session->app_process, 0);
                CloseHandle(session->app_process);
                return;
            }
        }

        // Input handler — target the edit child if found, otherwise main window
        session->input.set_target(edit_hwnd ? edit_hwnd : session->app_hwnd);

        session->running = true;

        // Capture thread — captures frames and encodes JPEG
        bool session_uses_vdd = use_vdd && session->vdd_capture.is_running();
        auto* raw_session = session.get();
        session->capture_thread = std::thread([raw_session, fps, jpeg_quality, session_uses_vdd]() {
            auto frame_interval = std::chrono::milliseconds(1000 / fps);
            while (raw_session->running && g_running) {
                auto t0 = std::chrono::steady_clock::now();
                webify::FrameData frame;
                bool got_frame = session_uses_vdd
                    ? raw_session->vdd_capture.capture_frame(frame)
                    : raw_session->capture.capture_frame(frame);
                if (got_frame) {
                    auto encoded = webify::encode_jpeg(frame.pixels.data(),
                                                        frame.width, frame.height,
                                                        jpeg_quality);
                    if (!encoded.empty()) {
                        std::lock_guard<std::mutex> lock(raw_session->frame_mutex);
                        raw_session->latest_jpeg = std::move(encoded);
                    }
                }
                auto elapsed = std::chrono::steady_clock::now() - t0;
                auto sleep_time = frame_interval - elapsed;
                if (sleep_time > std::chrono::milliseconds(0))
                    std::this_thread::sleep_for(sleep_time);
            }
            raw_session->capture.stop();
            raw_session->vdd_capture.stop();
        });

        {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            sessions[client_id] = std::move(session);
        }
    });

    ws.set_on_disconnect([&](int client_id) {
        fprintf(stdout, "Client %d disconnected — cleaning up\n", client_id);

        std::unique_ptr<ClientSession> session;
        {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            auto it = sessions.find(client_id);
            if (it != sessions.end()) {
                session = std::move(it->second);
                sessions.erase(it);
            }
        }

        if (session) {
            session->running = false;
            if (session->capture_thread.joinable())
                session->capture_thread.join();
            if (session->app_process) {
                TerminateProcess(session->app_process, 0);
                CloseHandle(session->app_process);
            }
            // Release the virtual monitor
            if (session->monitor_index >= 0) {
                std::lock_guard<std::mutex> lock(sessions_mutex);
                if (session->monitor_index < (int)monitor_in_use.size())
                    monitor_in_use[session->monitor_index] = false;
            }
            fprintf(stdout, "Client %d: session destroyed (monitor %d released)\n",
                    client_id, session->monitor_index);
        }
    });

    ws.set_on_message([&](int client_id, const std::string& msg, bool is_binary) {
        if (is_binary) return;
        std::string type = json_get_string(msg, "type");

        std::lock_guard<std::mutex> lock(sessions_mutex);
        auto it = sessions.find(client_id);
        if (it == sessions.end()) return;
        auto& session = it->second;

        if (type == "mousemove") {
            session->input.mouse_move(json_get_int(msg, "x"), json_get_int(msg, "y"));
        } else if (type == "mousedown") {
            session->input.mouse_button(json_get_int(msg, "button"), true);
        } else if (type == "mouseup") {
            session->input.mouse_button(json_get_int(msg, "button"), false);
        } else if (type == "wheel") {
            session->input.mouse_scroll(json_get_int(msg, "delta"));
        } else if (type == "keydown") {
            uint16_t vk = browser_keycode_to_vk(json_get_int(msg, "keyCode"),
                                                 json_get_string(msg, "code"));
            if (vk) session->input.key_event(vk, true);
        } else if (type == "keyup") {
            uint16_t vk = browser_keycode_to_vk(json_get_int(msg, "keyCode"),
                                                 json_get_string(msg, "code"));
            if (vk) session->input.key_event(vk, false);
        }
    });

    if (!ws.start(port)) {
        fprintf(stderr, "Failed to start WebSocket server on port %u\n", port);
        return 1;
    }

    fprintf(stdout, "\n>>> Open http://localhost:%u in your browser <<<\n", port);
    fprintf(stdout, ">>> Each tab/window gets its own %s instance <<<\n\n", app_path.c_str());

    // Frame broadcast loop — sends each client their own frames
    auto broadcast_interval = std::chrono::milliseconds(1000 / fps);

    while (g_running) {
        auto t0 = std::chrono::steady_clock::now();

        {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            for (auto& [cid, session] : sessions) {
                std::vector<uint8_t> jpeg_copy;
                {
                    std::lock_guard<std::mutex> flock(session->frame_mutex);
                    jpeg_copy = session->latest_jpeg;
                }
                if (!jpeg_copy.empty()) {
                    ws.send_binary(cid, jpeg_copy.data(), jpeg_copy.size());
                }
            }
        }

        auto elapsed = std::chrono::steady_clock::now() - t0;
        auto sleep_time = broadcast_interval - elapsed;
        if (sleep_time > std::chrono::milliseconds(0))
            std::this_thread::sleep_for(sleep_time);
    }

    // Shutdown — clean up all sessions
    fprintf(stdout, "\nShutting down...\n");
    ws.stop();
    {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        for (auto& [cid, session] : sessions) {
            session->running = false;
            if (session->capture_thread.joinable())
                session->capture_thread.join();
            if (session->app_process) {
                TerminateProcess(session->app_process, 0);
                CloseHandle(session->app_process);
            }
        }
        sessions.clear();
    }
#endif
    return 0;
}
