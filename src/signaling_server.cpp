#include "signaling_server.h"

#include <cstdio>

// For the PoC, we'll use a minimal WebSocket implementation.
// In production, you'd use a proper library like libwebsockets, uWebSockets, or Boost.Beast.
// For now, this is a placeholder that will be filled in with actual WebSocket code.

namespace webify {

struct SignalingServer::Impl {
    OnConnect on_connect;
    OnMessage on_message;
    OnDisconnect on_disconnect;
    uint16_t port = 0;
    // TODO: actual WebSocket server implementation
    // Options: uWebSockets, libwebsockets, or Boost.Beast
};

SignalingServer::SignalingServer() : impl_(new Impl()) {}

SignalingServer::~SignalingServer() {
    stop();
    delete impl_;
}

void SignalingServer::set_on_connect(OnConnect cb) {
    impl_->on_connect = std::move(cb);
}

void SignalingServer::set_on_message(OnMessage cb) {
    impl_->on_message = std::move(cb);
}

void SignalingServer::set_on_disconnect(OnDisconnect cb) {
    impl_->on_disconnect = std::move(cb);
}

bool SignalingServer::start(uint16_t port) {
    impl_->port = port;
    running_ = true;

    fprintf(stdout, "SignalingServer: listening on port %u (stub — needs WebSocket impl)\n", port);

    // TODO: Start actual WebSocket server
    // The server needs to:
    // 1. Accept WebSocket connections
    // 2. Handle SDP offer/answer exchange
    // 3. Relay ICE candidates
    // 4. Map client connections to desktop sessions

    return true;
}

void SignalingServer::stop() {
    if (!running_) return;
    running_ = false;
    fprintf(stdout, "SignalingServer: stopped\n");
}

void SignalingServer::send(const std::string& client_id, const std::string& message) {
    if (!running_) return;
    // TODO: send via WebSocket
    fprintf(stdout, "SignalingServer: send to %s: %s\n", client_id.c_str(), message.c_str());
}

} // namespace webify
