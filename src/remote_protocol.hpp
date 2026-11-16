#pragma once
#include <jansson.h>
#include <cctype>
#include <map>
#include <stdexcept>
#include <string>

struct HttpError { int status; };
struct RemoteRequest {
    std::string method, path, body;
    std::map<std::string, std::string> headers;
};
inline bool parseRemoteRequest(const std::string& wire, RemoteRequest& request) {
    const auto end = wire.find("\r\n\r\n");
    if (end == std::string::npos) { if (wire.size() > 8192) throw HttpError{413}; return false; }
    if (end > 8192) throw HttpError{413};
    const auto first = wire.find("\r\n");
    const auto space = wire.find(' '), second = wire.find(' ', space + 1);
    if (first == std::string::npos || space == std::string::npos || second == std::string::npos || second > first)
        throw HttpError{400};
    const auto version = wire.substr(second + 1, first - second - 1);
    if (version != "HTTP/1.1" && version != "HTTP/1.0") throw HttpError{400};
    request = {};
    request.method = wire.substr(0, space); request.path = wire.substr(space + 1, second - space - 1);
    if (request.path.empty() || request.path[0] != '/' || request.path.size() > 256) throw HttpError{400};
    for (size_t start = first + 2; start < end;) {
        const auto next = wire.find("\r\n", start);
        const auto colon = wire.find(':', start);
        if (next == std::string::npos || colon == std::string::npos || colon >= next || colon == start) throw HttpError{400};
        auto name = wire.substr(start, colon - start);
        for (auto& c : name) {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-') throw HttpError{400};
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        auto value = wire.substr(colon + 1, next - colon - 1);
        const auto a = value.find_first_not_of(" \t"), b = value.find_last_not_of(" \t");
        value = a == std::string::npos ? "" : value.substr(a, b - a + 1);
        for (auto c : value) if (static_cast<unsigned char>(c) < 32 && c != '\t') throw HttpError{400};
        if (!request.headers.emplace(name, value).second) throw HttpError{400};
        start = next + 2;
    }
    if (request.headers.count("transfer-encoding")) throw HttpError{400};
    size_t length = 0;
    auto cl = request.headers.find("content-length");
    if (cl != request.headers.end()) {
        if (cl->second.empty()) throw HttpError{400};
        for (char c : cl->second) {
            if (c < '0' || c > '9') throw HttpError{400};
            length = length * 10 + static_cast<size_t>(c - '0');
            if (length > 2048) throw HttpError{413};
        }
    }
    if (request.method == "POST" && cl == request.headers.end()) throw HttpError{411};
    if (wire.size() < end + 4 + length) return false;
    if (wire.size() != end + 4 + length) throw HttpError{400};
    request.body = wire.substr(end + 4, length);
    return true;
}
inline bool remoteAuthorized(const RemoteRequest& request, const std::string& token, const std::string& host) {
    auto h = request.headers.find("host");
    if (h == request.headers.end() || h->second != host) return false;
    auto origin = request.headers.find("origin");
    if (origin != request.headers.end() && origin->second != "http://" + host) return false;
    auto auth = request.headers.find("authorization");
    const auto expected = "Bearer " + token;
    if (auth == request.headers.end() || auth->second.size() != expected.size()) return false;
    unsigned difference = 0;
    for (size_t i = 0; i < expected.size(); ++i) difference |= static_cast<unsigned char>(expected[i] ^ auth->second[i]);
    return difference == 0;
}
struct RemoteCommand { std::string action, value; };
inline RemoteCommand parseRemoteCommand(const std::string& body) {
    if (body.size() > 2048) throw HttpError{413};
    auto root = json_loadb(body.data(), body.size(), JSON_REJECT_DUPLICATES, nullptr);
    if (!json_is_object(root)) { json_decref(root); throw HttpError{400}; }
    auto action = json_string_value(json_object_get(root, "action"));
    RemoteCommand command{action ? action : "", ""};
    if (action && json_string_length(json_object_get(root, "action")) != command.action.size()) { json_decref(root); throw HttpError{400}; }
    if (command.action == "search" || command.action == "play") {
        auto value = json_string_value(json_object_get(root, command.action == "search" ? "query" : "id"));
        if (!value) { json_decref(root); throw HttpError{400}; }
        command.value = value;
        if (json_string_length(json_object_get(root, command.action == "search" ? "query" : "id")) != command.value.size()) { json_decref(root); throw HttpError{400}; }
        if (command.value.size() > (command.action == "play" ? 256u : 512u)
            || (command.action == "play" && command.value.empty())) { json_decref(root); throw HttpError{400}; }
    } else if (command.action != "previous" && command.action != "next" && command.action != "toggle"
        && command.action != "stop" && command.action != "favorites" && command.action != "shuffle") {
        json_decref(root); throw HttpError{400};
    }
    json_decref(root);
    return command;
}
