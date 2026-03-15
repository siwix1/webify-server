#pragma once

#include <string>
#include <functional>
#include <cstdint>

namespace webify {

// Lightweight WebSocket server for WebRTC signaling.
// Handles SDP offer/answer exchange and ICE candidate relay.
// For the PoC, this is a simple HTTP + WebSocket server.
class SignalingServer {
public:
    using OnConnect = std::function<void(const std::string& client_id)>;
    using OnMessage = std::function<void(const std::string& client_id, const std::string& message)>;
    using OnDisconnect = std::function<void(const std::string& client_id)>;

    SignalingServer();
    ~SignalingServer();

    void set_on_connect(OnConnect cb);
    void set_on_message(OnMessage cb);
    void set_on_disconnect(OnDisconnect cb);

    // Start listening on the given port.
    bool start(uint16_t port = 8443);
    void stop();

    // Send a message to a specific client.
    void send(const std::string& client_id, const std::string& message);

    bool is_running() const { return running_; }

private:
    struct Impl;
    Impl* impl_ = nullptr;
    bool running_ = false;
};

} // namespace webify
