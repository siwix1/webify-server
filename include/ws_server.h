#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <unordered_map>

namespace webify {

// Minimal WebSocket server for the PoC.
// Handles HTTP upgrade, WebSocket framing, and binary/text messages.
class WsServer {
public:
    using OnConnect = std::function<void(int client_id)>;
    using OnMessage = std::function<void(int client_id, const std::string& msg, bool is_binary)>;
    using OnDisconnect = std::function<void(int client_id)>;

    WsServer();
    ~WsServer();

    void set_on_connect(OnConnect cb) { on_connect_ = std::move(cb); }
    void set_on_message(OnMessage cb) { on_message_ = std::move(cb); }
    void set_on_disconnect(OnDisconnect cb) { on_disconnect_ = std::move(cb); }

    // Set static content to serve on HTTP GET /
    void set_html(const std::string& html) { html_content_ = html; }

    bool start(uint16_t port);
    void stop();

    // Send a text message to a client
    void send_text(int client_id, const std::string& msg);

    // Send binary data to a client
    void send_binary(int client_id, const uint8_t* data, size_t len);

    // Broadcast binary data to all connected clients
    void broadcast_binary(const uint8_t* data, size_t len);

    bool is_running() const { return running_; }
    int client_count() const;

private:
    struct Client {
        SOCKET socket = INVALID_SOCKET;
        int id = 0;
        bool websocket = false;
        std::vector<uint8_t> recv_buf;
        std::thread thread;
        std::mutex send_mutex;
    };

    void accept_loop();
    void client_loop(int client_id);
    bool handle_http_request(Client& client, const std::string& request);
    bool do_websocket_handshake(Client& client, const std::string& request);
    bool read_websocket_frame(Client& client, std::string& payload, bool& is_binary);
    void send_websocket_frame(Client& client, const uint8_t* data, size_t len, uint8_t opcode);

    // Base64 and SHA1 for WebSocket handshake
    static std::string base64_encode(const uint8_t* data, size_t len);
    static std::vector<uint8_t> sha1(const std::string& input);

    SOCKET listen_socket_ = INVALID_SOCKET;
    std::atomic<bool> running_{false};
    std::thread accept_thread_;

    std::mutex clients_mutex_;
    std::unordered_map<int, std::unique_ptr<Client>> clients_;
    int next_client_id_ = 1;

    OnConnect on_connect_;
    OnMessage on_message_;
    OnDisconnect on_disconnect_;

    std::string html_content_;
};

} // namespace webify
