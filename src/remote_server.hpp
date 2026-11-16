#pragma once
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <deque>
#include <fstream>
#include <mutex>
#include <optional>
#include <thread>
#include "remote_protocol.hpp"

class RemoteServer {
    int listener = -1;
    unsigned short listenPort = 0;
    std::atomic<bool> active{false};
    std::thread worker;
    std::mutex mutex;
    std::deque<RemoteCommand> commands;
    std::string state = "{\"busy\":true}";
    std::string token, host, assets;
    void respond(int client, int status, const std::string& type, const std::string& body) {
        auto response = "HTTP/1.1 " + std::to_string(status) + " Response\r\nContent-Type: " + type
            + "\r\nContent-Length: " + std::to_string(body.size())
            + "\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff"
              "\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: default-src 'none'; script-src 'self'; style-src 'self'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'\r\n\r\n" + body;
        const auto start = std::chrono::steady_clock::now();
        for (size_t sent = 0; sent < response.size();) {
            if (std::chrono::steady_clock::now() - start > std::chrono::seconds(2)) break;
#ifdef MSG_NOSIGNAL
            constexpr int sendFlags = MSG_NOSIGNAL;
#else
            constexpr int sendFlags = 0; // libnx sockets do not raise POSIX SIGPIPE.
#endif
            const auto count = ::send(client, response.data() + sent, response.size() - sent, sendFlags);
            if (count <= 0) break;
            sent += static_cast<size_t>(count);
        }
    }
    void handle(int client) {
        timeval timeout{1, 0};
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        try {
            std::string wire;
            RemoteRequest request;
            const auto start = std::chrono::steady_clock::now();
            while (!parseRemoteRequest(wire, request)) {
                if (std::chrono::steady_clock::now() - start > std::chrono::seconds(2)) throw HttpError{408};
                char buffer[2048];
                const auto count = recv(client, buffer, sizeof(buffer), 0);
                if (count <= 0) throw HttpError{408};
                wire.append(buffer, static_cast<size_t>(count));
            }
            if (request.headers["host"] != host) throw HttpError{403};
            if (request.path == "/api/state" || request.path == "/api/command") {
                if (!remoteAuthorized(request, token, host)) throw HttpError{401};
                if (request.path == "/api/state" && request.method == "GET") {
                    std::string snapshot;
                    { std::lock_guard<std::mutex> lock(mutex); snapshot = state; }
                    respond(client, 200, "application/json; charset=utf-8", snapshot);
                } else if (request.path == "/api/command" && request.method == "POST") {
                    if (request.headers["content-type"].rfind("application/json", 0) != 0) throw HttpError{415};
                    auto command = parseRemoteCommand(request.body);
                    { std::lock_guard<std::mutex> lock(mutex);
                      if (commands.size() >= 16) throw HttpError{429};
                      commands.push_back(std::move(command)); }
                    respond(client, 202, "application/json", "{\"accepted\":true}");
                } else throw HttpError{405};
            } else {
                if (request.method != "GET") throw HttpError{405};
                const std::string file = request.path == "/" ? "index.html" : request.path == "/app.js" ? "app.js" : request.path == "/style.css" ? "style.css" : "";
                if (file.empty()) throw HttpError{404};
                std::ifstream input(assets + "/" + file, std::ios::binary);
                if (!input) throw HttpError{404};
                std::string body((std::istreambuf_iterator<char>(input)), {});
                if (body.size() > 128 * 1024) throw HttpError{500};
                respond(client, 200, file == "app.js" ? "text/javascript; charset=utf-8" : file == "style.css" ? "text/css; charset=utf-8" : "text/html; charset=utf-8", body);
            }
        } catch (const HttpError& error) { respond(client, error.status, "application/json", "{\"error\":true}"); }
        catch (...) { respond(client, 500, "application/json", "{\"error\":true}"); }
        close(client);
    }
public:
    ~RemoteServer() { stop(); }
    void start(std::string code, std::string address, std::string assetDirectory, unsigned short port = 8080) {
        stop();
        in_addr parsed;
        if (code.size() != 32 || inet_pton(AF_INET, address.c_str(), &parsed) != 1 || parsed.s_addr == INADDR_ANY)
            throw std::runtime_error("remote_network");
        listener = socket(AF_INET, SOCK_STREAM, 0);
        if (listener < 0) throw std::runtime_error("remote_start_error");
        int reuse = 1; setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        sockaddr_in endpoint{}; endpoint.sin_family = AF_INET; endpoint.sin_port = htons(port); endpoint.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(listener, reinterpret_cast<sockaddr*>(&endpoint), sizeof(endpoint)) != 0 || listen(listener, 4) != 0) {
            close(listener); listener = -1; throw std::runtime_error("remote_start_error");
        }
        socklen_t length = sizeof(endpoint); getsockname(listener, reinterpret_cast<sockaddr*>(&endpoint), &length);
        listenPort = ntohs(endpoint.sin_port); host = address + ":" + std::to_string(listenPort);
        token = std::move(code); assets = std::move(assetDirectory);
        { std::lock_guard<std::mutex> lock(mutex); commands.clear(); state = "{\"busy\":true}"; }
        active = true;
        try {
            worker = std::thread([this] {
                while (active) {
                    fd_set sockets; FD_ZERO(&sockets); FD_SET(listener, &sockets); timeval wait{0, 200000};
                    if (select(listener + 1, &sockets, nullptr, nullptr, &wait) <= 0) continue;
                    const int client = accept(listener, nullptr, nullptr);
                    if (client >= 0) handle(client);
                }
            });
        } catch (...) { active = false; close(listener); listener = -1; throw; }
    }
    void stop() {
        active = false;
        if (listener >= 0) shutdown(listener, SHUT_RDWR);
        if (worker.joinable()) worker.join();
        if (listener >= 0) close(listener);
        listener = -1;
        std::lock_guard<std::mutex> lock(mutex); commands.clear();
    }
    bool running() const { return active; }
    unsigned short port() const { return listenPort; }
    std::string url() const { return "http://" + host; }
    std::string pairingCode() const { return token; }
    void publish(std::string snapshot) { std::lock_guard<std::mutex> lock(mutex); state = std::move(snapshot); }
    std::optional<RemoteCommand> take() {
        std::lock_guard<std::mutex> lock(mutex);
        if (commands.empty()) return {};
        auto command = std::move(commands.front()); commands.pop_front(); return command;
    }
};
