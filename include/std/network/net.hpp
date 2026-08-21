#pragma once

#include <iostream>
#include <string>
#include <cstring>

#if defined(_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using socket_t = SOCKET;
    #define CLOSE_SOCKET(s) closesocket(s)
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    using socket_t = int;
    #define CLOSE_SOCKET(s) ::close(s)
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR   (-1)
#endif

namespace zync {
namespace stdlib {

struct TcpStream {
    socket_t fd = INVALID_SOCKET;

    bool is_valid() const {
#if defined(_WIN32)
        return fd != INVALID_SOCKET;
#else
        return fd >= 0;
#endif
    }

    bool send(const std::string& msg) const {
        if (!is_valid()) return false;
        return ::send(fd, msg.c_str(), static_cast<int>(msg.length()), 0) != SOCKET_ERROR;
    }

    std::string receive(int max_len = 4096) const {
        if (!is_valid()) return "";
        std::string buffer(max_len, '\0');
        auto bytes = ::recv(fd, &buffer[0], max_len - 1, 0);
        if (bytes <= 0) return "";
        buffer.resize(bytes);
        return buffer;
    }

    void close() {
        if (is_valid()) {
            CLOSE_SOCKET(fd);
            fd = INVALID_SOCKET;
        }
    }
};

struct TcpListener {
    socket_t server_fd = INVALID_SOCKET;

    bool is_valid() const {
#if defined(_WIN32)
        return server_fd != INVALID_SOCKET;
#else
        return server_fd >= 0;
#endif
    }

    static TcpListener bind(int port) {
        TcpListener listener;

#if defined(_WIN32)
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return listener;
        }
#endif

        listener.server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listener.server_fd == INVALID_SOCKET) return listener;

        int opt = 1;
#if defined(_WIN32)
        ::setsockopt(listener.server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
        ::setsockopt(listener.server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(static_cast<uint16_t>(port));

        if (::bind(listener.server_fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
            CLOSE_SOCKET(listener.server_fd);
            listener.server_fd = INVALID_SOCKET;
            return listener;
        }

        if (::listen(listener.server_fd, 10) == SOCKET_ERROR) {
            CLOSE_SOCKET(listener.server_fd);
            listener.server_fd = INVALID_SOCKET;
        }

        return listener;
    }

    TcpStream accept() const {
        if (!is_valid()) return TcpStream{INVALID_SOCKET};
        sockaddr_in client_addr{};
        socklen_t addrlen = sizeof(client_addr);
        socket_t client_fd = ::accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &addrlen);
        return TcpStream{client_fd};
    }

    void close() {
        if (is_valid()) {
            CLOSE_SOCKET(server_fd);
            server_fd = INVALID_SOCKET;
        }
    }
};

} // namespace stdlib
} // namespace zync

using TcpStream = zync::stdlib::TcpStream;
using TcpListener = zync::stdlib::TcpListener;