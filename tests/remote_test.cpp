#include "remote_server.hpp"
#include <cassert>
#include <iostream>

template<class F> void rejected(int status, F fn) {
    bool failed = false;
    try { fn(); } catch (const HttpError& error) { failed = error.status == status; }
    assert(failed);
}
std::string sendRequest(unsigned short port, const std::string& request) {
    const int client = socket(AF_INET, SOCK_STREAM, 0); assert(client >= 0);
    timeval limit{3, 0}; setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &limit, sizeof(limit));
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_port = htons(port); address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(connect(client, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
    assert(send(client, request.data(), request.size(), MSG_NOSIGNAL) == static_cast<ssize_t>(request.size()));
    std::string response; char buffer[4096]; ssize_t count;
    while ((count = recv(client, buffer, sizeof(buffer), 0)) > 0) response.append(buffer, static_cast<size_t>(count));
    close(client); return response;
}
int main() {
    RemoteRequest request;
    const std::string body = R"({"action":"search","query":"AC/DC & <test>"})";
    const auto wire = "POST /api/command HTTP/1.1\r\nHost: 127.0.0.1:8080\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    for (size_t i = 0; i < wire.size(); ++i) assert(!parseRemoteRequest(wire.substr(0, i), request));
    assert(parseRemoteRequest(wire, request));
    assert(parseRemoteCommand(request.body).value == "AC/DC & <test>");
    rejected(400, [&] { parseRemoteRequest("POST /api/command HTTP/1.1\r\nContent-Length: 0\r\nContent-Length: 0\r\n\r\n", request); });
    rejected(400, [&] { parseRemoteRequest("POST /api/command HTTP/1.1\r\nTransfer-Encoding: chunked\r\nContent-Length: 0\r\n\r\n", request); });
    rejected(413, [&] { parseRemoteRequest("POST /api/command HTTP/1.1\r\nContent-Length: 99999999999999999999\r\n\r\n", request); });
    rejected(400, [&] { parseRemoteCommand(R"({"action":"shell"})"); });
    rejected(400, [&] { parseRemoteCommand(R"({"action":"play","id":""})"); });
    rejected(400, [&] { parseRemoteCommand(R"({"action":"next\u0000stop"})"); });
    rejected(400, [&] { parseRemoteCommand(R"({"action":"play","id":"a\u0000b"})"); });
    rejected(400, [&] { parseRemoteCommand(R"({"action":"next","action":"stop"})"); });
    RemoteServer server;
    const std::string code = "0123456789abcdef0123456789abcdef";
    server.start(code, "127.0.0.1", "resources/remote", 0);
    const auto host = "127.0.0.1:" + std::to_string(server.port());
    server.publish(R"({"busy":false,"current":{"title":"Test track"}})");
    auto get = [&](const std::string& path, const std::string& auth = "", const std::string& extra = "") {
        return sendRequest(server.port(), "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\n" + auth + extra + "\r\n");
    };
    assert(get("/").find("200 Response") != std::string::npos);
    assert(get("/../config.json").find("404 Response") != std::string::npos);
    assert(get("/api/state").find("401 Response") != std::string::npos);
    const auto auth = "Authorization: Bearer " + code + "\r\n";
    const auto state = get("/api/state", auth);
    assert(state.find("Test track") != std::string::npos && state.find("Content-Security-Policy") != std::string::npos);
    assert(get("/api/state", auth, "Origin: http://evil.example\r\n").find("401 Response") != std::string::npos);
    auto post = [&](const std::string& payload) {
        return sendRequest(server.port(), "POST /api/command HTTP/1.1\r\nHost: " + host + "\r\n" + auth + "Content-Type: application/json\r\nContent-Length: " + std::to_string(payload.size()) + "\r\n\r\n" + payload);
    };
    assert(post(R"({"action":"next"})").find("202 Response") != std::string::npos);
    auto command = server.take(); assert(command && command->action == "next");
    for (int i = 0; i < 16; ++i) assert(post(R"({"action":"toggle"})").find("202 Response") != std::string::npos);
    assert(post(R"({"action":"toggle"})").find("429 Response") != std::string::npos);
    server.stop(); assert(!server.running());
    std::cout << "Remote protocol, authorization, server and queue checks passed\n";
}
