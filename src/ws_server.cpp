#include "ws_server.h"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <sstream>

namespace webify {

// ---- SHA1 implementation (minimal, for WebSocket handshake only) ----

static uint32_t sha1_rol(uint32_t value, int bits) {
    return (value << bits) | (value >> (32 - bits));
}

std::vector<uint8_t> WsServer::sha1(const std::string& input) {
    uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE,
             h3 = 0x10325476, h4 = 0xC3D2E1F0;

    std::vector<uint8_t> msg(input.begin(), input.end());
    uint64_t bit_len = msg.size() * 8;
    msg.push_back(0x80);
    while (msg.size() % 64 != 56) msg.push_back(0);
    for (int i = 7; i >= 0; i--) msg.push_back((bit_len >> (i * 8)) & 0xFF);

    for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = (msg[chunk+i*4]<<24)|(msg[chunk+i*4+1]<<16)|
                   (msg[chunk+i*4+2]<<8)|msg[chunk+i*4+3];
        for (int i = 16; i < 80; i++)
            w[i] = sha1_rol(w[i-3]^w[i-8]^w[i-14]^w[i-16], 1);

        uint32_t a=h0, b=h1, c=h2, d=h3, e=h4;
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20)      { f=(b&c)|((~b)&d); k=0x5A827999; }
            else if (i < 40) { f=b^c^d;           k=0x6ED9EBA1; }
            else if (i < 60) { f=(b&c)|(b&d)|(c&d); k=0x8F1BBCDC; }
            else              { f=b^c^d;           k=0xCA62C1D6; }
            uint32_t temp = sha1_rol(a,5)+f+e+k+w[i];
            e=d; d=c; c=sha1_rol(b,30); b=a; a=temp;
        }
        h0+=a; h1+=b; h2+=c; h3+=d; h4+=e;
    }

    std::vector<uint8_t> hash(20);
    for (int i = 0; i < 4; i++) {
        hash[i]    = (h0>>(24-i*8))&0xFF;
        hash[i+4]  = (h1>>(24-i*8))&0xFF;
        hash[i+8]  = (h2>>(24-i*8))&0xFF;
        hash[i+12] = (h3>>(24-i*8))&0xFF;
        hash[i+16] = (h4>>(24-i*8))&0xFF;
    }
    return hash;
}

// ---- Base64 ----

std::string WsServer::base64_encode(const uint8_t* data, size_t len) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = (uint32_t)data[i] << 16;
        if (i+1 < len) n |= (uint32_t)data[i+1] << 8;
        if (i+2 < len) n |= data[i+2];
        out += table[(n>>18)&0x3F];
        out += table[(n>>12)&0x3F];
        out += (i+1 < len) ? table[(n>>6)&0x3F] : '=';
        out += (i+2 < len) ? table[n&0x3F] : '=';
    }
    return out;
}

// ---- WsServer ----

WsServer::WsServer() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
}

WsServer::~WsServer() {
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool WsServer::start(uint16_t port) {
    listen_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket_ == INVALID_SOCKET) {
        fprintf(stderr, "WsServer: socket() failed\n");
        return false;
    }

    int opt = 1;
    setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listen_socket_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        fprintf(stderr, "WsServer: bind() failed\n");
        closesocket(listen_socket_);
        listen_socket_ = INVALID_SOCKET;
        return false;
    }

    if (listen(listen_socket_, SOMAXCONN) == SOCKET_ERROR) {
        fprintf(stderr, "WsServer: listen() failed\n");
        closesocket(listen_socket_);
        listen_socket_ = INVALID_SOCKET;
        return false;
    }

    running_ = true;
    accept_thread_ = std::thread(&WsServer::accept_loop, this);

    fprintf(stdout, "WsServer: listening on port %u\n", port);
    return true;
}

void WsServer::stop() {
    running_ = false;

    if (listen_socket_ != INVALID_SOCKET) {
        closesocket(listen_socket_);
        listen_socket_ = INVALID_SOCKET;
    }

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    // Close all client connections
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (auto& [id, client] : clients_) {
        if (client->socket != INVALID_SOCKET) {
            closesocket(client->socket);
            client->socket = INVALID_SOCKET;
        }
        if (client->thread.joinable()) {
            client->thread.join();
        }
    }
    clients_.clear();
}

int WsServer::client_count() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(clients_mutex_));
    int count = 0;
    for (auto& [id, client] : clients_) {
        if (client->websocket) count++;
    }
    return count;
}

void WsServer::accept_loop() {
    while (running_) {
        sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        SOCKET client_sock = accept(listen_socket_, (sockaddr*)&client_addr,
#ifdef _WIN32
                                     &addr_len
#else
                                     (socklen_t*)&addr_len
#endif
        );

        if (client_sock == INVALID_SOCKET) {
            if (running_) {
                fprintf(stderr, "WsServer: accept() failed\n");
            }
            continue;
        }

        int client_id;
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            client_id = next_client_id_++;
            auto client = std::make_unique<Client>();
            client->socket = client_sock;
            client->id = client_id;
            client->thread = std::thread(&WsServer::client_loop, this, client_id);
            clients_[client_id] = std::move(client);
        }
    }
}

void WsServer::client_loop(int client_id) {
    Client* client = nullptr;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = clients_.find(client_id);
        if (it == clients_.end()) return;
        client = it->second.get();
    }

    // Read initial HTTP request
    char buf[4096];
    std::string request;

    while (running_ && client->socket != INVALID_SOCKET) {
        int n = recv(client->socket, buf, sizeof(buf), 0);
        if (n <= 0) break;

        if (!client->websocket) {
            request.append(buf, n);
            // Wait for complete HTTP headers
            if (request.find("\r\n\r\n") != std::string::npos) {
                if (!handle_http_request(*client, request)) {
                    break;
                }
                if (!client->websocket) {
                    break;  // Was a regular HTTP request, done
                }
                request.clear();

                if (on_connect_) on_connect_(client_id);
            }
        } else {
            // WebSocket mode — parse frames
            client->recv_buf.insert(client->recv_buf.end(), buf, buf + n);

            while (true) {
                std::string payload;
                bool is_binary;
                // Save buffer state in case we don't have a complete frame
                auto saved_buf = client->recv_buf;
                if (!read_websocket_frame(*client, payload, is_binary)) {
                    client->recv_buf = saved_buf;
                    break;
                }
                if (on_message_) on_message_(client_id, payload, is_binary);
            }
        }
    }

    if (client->websocket && on_disconnect_) {
        on_disconnect_(client_id);
    }

    if (client->socket != INVALID_SOCKET) {
        closesocket(client->socket);
        client->socket = INVALID_SOCKET;
    }
}

bool WsServer::handle_http_request(Client& client, const std::string& request) {
    // Check if this is a WebSocket upgrade
    if (request.find("Upgrade: websocket") != std::string::npos ||
        request.find("Upgrade: Websocket") != std::string::npos) {
        return do_websocket_handshake(client, request);
    }

    // Regular HTTP — serve the HTML page
    std::string body = html_content_.empty() ?
        "<html><body><h1>webify-server</h1></body></html>" : html_content_;

    std::ostringstream resp;
    resp << "HTTP/1.1 200 OK\r\n"
         << "Content-Type: text/html\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Connection: close\r\n"
         << "\r\n"
         << body;

    std::string r = resp.str();
    ::send(client.socket, r.c_str(), (int)r.size(), 0);
    return false;  // close after response
}

bool WsServer::do_websocket_handshake(Client& client, const std::string& request) {
    // Extract Sec-WebSocket-Key
    std::string key;
    auto pos = request.find("Sec-WebSocket-Key: ");
    if (pos == std::string::npos) {
        pos = request.find("Sec-WebSocket-Key:");
    }
    if (pos != std::string::npos) {
        auto start = request.find(':', pos) + 1;
        while (start < request.size() && request[start] == ' ') start++;
        auto end = request.find("\r\n", start);
        key = request.substr(start, end - start);
    }

    if (key.empty()) {
        fprintf(stderr, "WsServer: No WebSocket key in handshake\n");
        return false;
    }

    // Compute accept key: SHA1(key + magic) -> base64
    std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    auto hash = sha1(key + magic);
    std::string accept = base64_encode(hash.data(), hash.size());

    std::ostringstream resp;
    resp << "HTTP/1.1 101 Switching Protocols\r\n"
         << "Upgrade: websocket\r\n"
         << "Connection: Upgrade\r\n"
         << "Sec-WebSocket-Accept: " << accept << "\r\n"
         << "\r\n";

    std::string r = resp.str();
    ::send(client.socket, r.c_str(), (int)r.size(), 0);

    client.websocket = true;
    fprintf(stdout, "WsServer: client %d upgraded to WebSocket\n", client.id);
    return true;
}

bool WsServer::read_websocket_frame(Client& client, std::string& payload, bool& is_binary) {
    auto& buf = client.recv_buf;
    if (buf.size() < 2) return false;

    uint8_t b0 = buf[0], b1 = buf[1];
    uint8_t opcode = b0 & 0x0F;
    bool masked = (b1 & 0x80) != 0;
    uint64_t payload_len = b1 & 0x7F;
    size_t header_len = 2;

    if (payload_len == 126) {
        if (buf.size() < 4) return false;
        payload_len = ((uint64_t)buf[2] << 8) | buf[3];
        header_len = 4;
    } else if (payload_len == 127) {
        if (buf.size() < 10) return false;
        payload_len = 0;
        for (int i = 0; i < 8; i++)
            payload_len = (payload_len << 8) | buf[2 + i];
        header_len = 10;
    }

    size_t mask_len = masked ? 4 : 0;
    size_t total = header_len + mask_len + payload_len;
    if (buf.size() < total) return false;

    // Handle close frame
    if (opcode == 0x08) {
        buf.erase(buf.begin(), buf.begin() + total);
        return false;
    }

    // Handle ping — send pong
    if (opcode == 0x09) {
        std::vector<uint8_t> pong_data(buf.begin() + header_len + mask_len,
                                        buf.begin() + total);
        if (masked) {
            uint8_t mask[4] = {buf[header_len], buf[header_len+1],
                               buf[header_len+2], buf[header_len+3]};
            for (size_t i = 0; i < pong_data.size(); i++)
                pong_data[i] ^= mask[i % 4];
        }
        send_websocket_frame(client, pong_data.data(), pong_data.size(), 0x0A);
        buf.erase(buf.begin(), buf.begin() + total);
        return false;
    }

    is_binary = (opcode == 0x02);

    // Extract and unmask payload
    payload.resize(payload_len);
    uint8_t mask[4] = {0, 0, 0, 0};
    if (masked) {
        for (int i = 0; i < 4; i++)
            mask[i] = buf[header_len + i];
    }

    for (uint64_t i = 0; i < payload_len; i++) {
        payload[i] = buf[header_len + mask_len + i] ^ mask[i % 4];
    }

    buf.erase(buf.begin(), buf.begin() + total);
    return true;
}

void WsServer::send_websocket_frame(Client& client, const uint8_t* data, size_t len, uint8_t opcode) {
    std::vector<uint8_t> frame;

    // FIN + opcode
    frame.push_back(0x80 | opcode);

    // Length (server->client is never masked)
    if (len < 126) {
        frame.push_back((uint8_t)len);
    } else if (len < 65536) {
        frame.push_back(126);
        frame.push_back((len >> 8) & 0xFF);
        frame.push_back(len & 0xFF);
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; i--)
            frame.push_back((len >> (i * 8)) & 0xFF);
    }

    frame.insert(frame.end(), data, data + len);

    std::lock_guard<std::mutex> lock(client.send_mutex);
    if (client.socket != INVALID_SOCKET) {
        ::send(client.socket, (const char*)frame.data(), (int)frame.size(), 0);
    }
}

void WsServer::send_text(int client_id, const std::string& msg) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    auto it = clients_.find(client_id);
    if (it == clients_.end() || !it->second->websocket) return;
    send_websocket_frame(*it->second, (const uint8_t*)msg.data(), msg.size(), 0x01);
}

void WsServer::send_binary(int client_id, const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    auto it = clients_.find(client_id);
    if (it == clients_.end() || !it->second->websocket) return;
    send_websocket_frame(*it->second, data, len, 0x02);
}

void WsServer::broadcast_binary(const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (auto& [id, client] : clients_) {
        if (client->websocket && client->socket != INVALID_SOCKET) {
            send_websocket_frame(*client, data, len, 0x02);
        }
    }
}

} // namespace webify
