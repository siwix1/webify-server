#include "desktop_manager.h"
#include "screen_capture.h"

#include <cstdio>
#include <thread>
#include <chrono>

int main() {
    fprintf(stdout, "=== Desktop Creation Test ===\n\n");

    webify::DesktopManager mgr;

    // Test 1: Create a desktop and launch notepad
    fprintf(stdout, "Test 1: Create desktop + launch notepad\n");
    std::string session_id = mgr.create_session("notepad.exe");

    if (session_id.empty()) {
        fprintf(stderr, "FAIL: Could not create session\n");
        return 1;
    }
    fprintf(stdout, "  PASS: Session created: %s\n", session_id.c_str());

    const auto* session = mgr.get_session(session_id);
    if (!session) {
        fprintf(stderr, "FAIL: Could not get session info\n");
        return 1;
    }
    fprintf(stdout, "  Desktop: %s\n", session->desktop_name.c_str());

    // Test 2: Capture a frame from the desktop
    fprintf(stdout, "\nTest 2: Capture frame from desktop\n");

    // Give notepad a moment to render
    std::this_thread::sleep_for(std::chrono::seconds(2));

    webify::ScreenCapture capture;
    if (!capture.start(session->desktop_name, session->width, session->height)) {
        fprintf(stderr, "FAIL: Could not start screen capture\n");
        mgr.destroy_session(session_id);
        return 1;
    }
    fprintf(stdout, "  PASS: Screen capture started\n");

    webify::FrameData frame;
    if (!capture.capture_frame(frame)) {
        fprintf(stderr, "FAIL: Could not capture frame\n");
        capture.stop();
        mgr.destroy_session(session_id);
        return 1;
    }
    fprintf(stdout, "  PASS: Frame captured: %dx%d (%zu bytes)\n",
            frame.width, frame.height, frame.pixels.size());

    // Check if frame has any non-zero pixels (i.e., something was rendered)
    bool has_content = false;
    for (size_t i = 0; i < frame.pixels.size(); i += 4) {
        if (frame.pixels[i] != 0 || frame.pixels[i+1] != 0 || frame.pixels[i+2] != 0) {
            has_content = true;
            break;
        }
    }
    fprintf(stdout, "  %s: Frame has visible content\n", has_content ? "PASS" : "WARN");

    // Test 3: Multiple sessions
    fprintf(stdout, "\nTest 3: Multiple concurrent sessions\n");
    std::string session2 = mgr.create_session("notepad.exe");
    std::string session3 = mgr.create_session("notepad.exe");

    auto sessions = mgr.list_sessions();
    fprintf(stdout, "  Active sessions: %zu\n", sessions.size());
    fprintf(stdout, "  %s: Multiple sessions created\n",
            sessions.size() == 3 ? "PASS" : "FAIL");

    // Cleanup
    capture.stop();
    mgr.destroy_session(session_id);
    mgr.destroy_session(session2);
    mgr.destroy_session(session3);

    fprintf(stdout, "\n=== All tests completed ===\n");
    return 0;
}
