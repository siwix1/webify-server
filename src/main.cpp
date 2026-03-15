#include "desktop_manager.h"
#include "screen_capture.h"
#include "input_handler.h"
#include "encoder.h"
#include "signaling_server.h"

#include <cstdio>
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>

static std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running = false;
}

int main(int argc, char* argv[]) {
    fprintf(stdout, "=== webify-server ===\n");
    fprintf(stdout, "Desktop streaming server for legacy Win32 applications\n\n");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Parse command line
    std::string app_path = "notepad.exe";  // default test app
    int width = 1024;
    int height = 768;
    int fps = 15;
    uint16_t port = 8443;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--app" && i + 1 < argc) {
            app_path = argv[++i];
        } else if (arg == "--width" && i + 1 < argc) {
            width = std::stoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            height = std::stoi(argv[++i]);
        } else if (arg == "--fps" && i + 1 < argc) {
            fps = std::stoi(argv[++i]);
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--help") {
            fprintf(stdout, "Usage: webify-server [options]\n");
            fprintf(stdout, "  --app <path>     Application to launch (default: notepad.exe)\n");
            fprintf(stdout, "  --width <px>     Desktop width (default: 1024)\n");
            fprintf(stdout, "  --height <px>    Desktop height (default: 768)\n");
            fprintf(stdout, "  --fps <n>        Capture framerate (default: 15)\n");
            fprintf(stdout, "  --port <n>       Signaling port (default: 8443)\n");
            return 0;
        }
    }

    // Initialize components
    webify::DesktopManager desktop_mgr;
    webify::Encoder encoder;
    webify::SignalingServer signaling;

    // Start signaling server
    if (!signaling.start(port)) {
        fprintf(stderr, "Failed to start signaling server\n");
        return 1;
    }

    // Create a desktop session and launch the app
    fprintf(stdout, "Creating desktop session for: %s\n", app_path.c_str());
    std::string session_id = desktop_mgr.create_session(app_path, "", width, height);

    if (session_id.empty()) {
        fprintf(stderr, "Failed to create desktop session\n");
        return 1;
    }

    const auto* session = desktop_mgr.get_session(session_id);
    fprintf(stdout, "Session active: %s (desktop: %s)\n",
            session_id.c_str(), session->desktop_name.c_str());

    // Initialize screen capture on the new desktop
    webify::ScreenCapture capture;
    if (!capture.start(session->desktop_name, width, height, fps)) {
        fprintf(stderr, "Failed to start screen capture\n");
        desktop_mgr.destroy_session(session_id);
        return 1;
    }

    // Initialize encoder
    if (!encoder.init(width, height, fps)) {
        fprintf(stderr, "Failed to initialize encoder\n");
        desktop_mgr.destroy_session(session_id);
        return 1;
    }

    // Main capture loop
    fprintf(stdout, "\nStreaming started. Press Ctrl+C to stop.\n");
    auto frame_interval = std::chrono::milliseconds(1000 / fps);
    uint64_t frames_captured = 0;

    while (g_running) {
        auto frame_start = std::chrono::steady_clock::now();

        // Capture frame
        webify::FrameData frame;
        if (capture.capture_frame(frame)) {
            // Encode
            webify::EncodedPacket packet;
            if (encoder.encode(frame, packet)) {
                // TODO: send packet via WebRTC to connected clients
                frames_captured++;

                if (frames_captured % (fps * 5) == 0) {
                    fprintf(stdout, "Frames captured: %llu, last packet: %zu bytes\n",
                            static_cast<unsigned long long>(frames_captured),
                            packet.data.size());
                }
            }
        }

        // Sleep to maintain framerate
        auto elapsed = std::chrono::steady_clock::now() - frame_start;
        auto sleep_time = frame_interval - elapsed;
        if (sleep_time > std::chrono::milliseconds(0)) {
            std::this_thread::sleep_for(sleep_time);
        }
    }

    // Cleanup
    fprintf(stdout, "\nShutting down...\n");
    encoder.shutdown();
    capture.stop();
    signaling.stop();
    desktop_mgr.destroy_session(session_id);

    fprintf(stdout, "Total frames captured: %llu\n",
            static_cast<unsigned long long>(frames_captured));
    fprintf(stdout, "Goodbye.\n");

    return 0;
}
