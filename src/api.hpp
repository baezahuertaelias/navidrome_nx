#pragma once
#include <curl/curl.h>
#include <jansson.h>
#include <mbedtls/md5.h>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using Json = std::unique_ptr<json_t, decltype(&json_decref)>;
inline Json jsonOwner(json_t* value) { return Json(value, json_decref); }
inline std::string field(json_t* object, const char* key) {
    auto value = json_string_value(json_object_get(object, key));
    if (!value) throw std::runtime_error(std::string("Missing text field: ") + key);
    return value;
}
struct Entry { std::string id, title, artist; long long duration = 0; std::string coverArt; };
struct Transfer {
    std::vector<char> bytes;
    size_t limit;
    bool overflow = false;
    static size_t write(char* data, size_t size, size_t count, void* context) {
        auto& out = *static_cast<Transfer*>(context);
        if (size && count > (out.limit - out.bytes.size()) / size) {
            out.overflow = true;
            return 0;
        }
        try { out.bytes.insert(out.bytes.end(), data, data + size * count); }
        catch (...) { return 0; }
        return size * count;
    }
};

class Api {
    std::string server, username, password;
    std::atomic<bool> cancelled{false};
    static int progress(void* context, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
        return static_cast<const Api*>(context)->cancelled.load() ? 1 : 0;
    }
public:
    void cancel() { cancelled.store(true); }
    explicit Api(json_t* config) : server(field(config, "server")),
        username(field(config, "username")), password(field(config, "password")) {
        while (!server.empty() && server.back() == '/') server.pop_back();
        if ((server.rfind("https://", 0) != 0 && server.rfind("http://", 0) != 0)
            || server.find_first_of("?#\r\n") != std::string::npos || server.find('@') != std::string::npos
            || username.empty() || password.empty())
            throw std::runtime_error("Config needs an HTTP(S) server URL, username and password");
    }
    static std::string encode(CURL* curl, const std::string& text) {
        char* encoded = curl_easy_escape(curl, text.c_str(), static_cast<int>(text.size()));
        if (!encoded) throw std::runtime_error("URL encoding failed");
        std::string result(encoded);
        curl_free(encoded);
        return result;
    }
    std::vector<char> request(const std::string& endpoint,
        const std::vector<std::pair<std::string, std::string>>& params,
        const std::string& salt, size_t limit = 4 * 1024 * 1024) const {
        std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
        if (!curl) throw std::runtime_error("Cannot create HTTP client");
        unsigned char digest[16];
        const auto input = password + salt;
        if (mbedtls_md5_ret(reinterpret_cast<const unsigned char*>(input.data()), input.size(), digest))
            throw std::runtime_error("Authentication hash failed");
        std::string token;
        for (auto byte : digest) {
            token += "0123456789abcdef"[byte >> 4];
            token += "0123456789abcdef"[byte & 15];
        }
        auto url = server + "/rest/" + endpoint + ".view?u=" + encode(curl.get(), username)
            + "&t=" + token + "&s=" + encode(curl.get(), salt) + "&v=1.16.1&c=NavidromeNX&f=json";
        for (const auto& param : params)
            url += "&" + encode(curl.get(), param.first) + "=" + encode(curl.get(), param.second);
        Transfer output{{}, limit};
        curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
#if LIBCURL_VERSION_NUM >= 0x075500
        curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "http,https");
#else
        curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif
        // Redirects are deliberately rejected so authentication tokens stay on this server.
        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, endpoint == "stream" ? 180L : 30L);
        curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, progress);
        curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, this);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, Transfer::write);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &output);
        auto result = curl_easy_perform(curl.get());
        if (output.overflow) throw std::runtime_error("Server response exceeds memory limit");
        if (result != CURLE_OK) throw std::runtime_error(std::string("Network: ") + curl_easy_strerror(result));
        long status = 0;
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
        if (status != 200) throw std::runtime_error("HTTP " + std::to_string(status));
        return std::move(output.bytes);
    }
    static Json response(const std::vector<char>& bytes) {
        auto root = jsonOwner(json_loadb(bytes.data(), bytes.size(), JSON_REJECT_DUPLICATES, nullptr));
        auto body = json_object_get(root.get(), "subsonic-response");
        if (!body) throw std::runtime_error("Invalid Subsonic response");
        if (field(body, "status") != "ok") {
            auto error = json_object_get(body, "error");
            auto code = json_integer_value(json_object_get(error, "code"));
            throw std::runtime_error("Subsonic error " + std::to_string(code) + "; check server credentials/settings");
        }
        return jsonOwner(json_incref(body));
    }
    static std::vector<Entry> entries(json_t* body, const char* group, const char* item) {
        auto array = json_object_get(json_object_get(body, group), item);
        if (!array) return {};
        if (!json_is_array(array)) throw std::runtime_error("Invalid library response");
        std::vector<Entry> result;
        size_t index; json_t* value;
        json_array_foreach(array, index, value) {
            auto artist = json_string_value(json_object_get(value, "artist"));
            auto duration = json_integer_value(json_object_get(value, "duration"));
            auto cover = json_string_value(json_object_get(value, "coverArt"));
            result.push_back({field(value, "id"), field(value, std::string(item) == "album" ? "name" : "title"), artist ? artist : "Unknown artist", duration > 0 ? duration : 0, cover ? cover : ""});
        }
        return result;
    }
    static std::vector<std::string> lyrics(json_t* body) {
        std::vector<std::string> result;
        auto structured = json_object_get(json_object_get(body, "lyricsList"), "structuredLyrics");
        size_t index; json_t* track;
        json_array_foreach(structured, index, track) {
            auto lines = json_object_get(track, "line");
            size_t lineIndex; json_t* line;
            json_array_foreach(lines, lineIndex, line) {
                auto text = json_string_value(json_object_get(line, "value"));
                if (text) {
                    if (result.size() >= 2000) throw std::runtime_error("Lyrics exceed 2000 lines");
                    result.emplace_back(text);
                }
            }
            if (!result.empty()) return result; // Display the first available language/version.
        }
        auto value = json_string_value(json_object_get(json_object_get(body, "lyrics"), "value"));
        if (value) {
            std::string text(value);
            size_t start = 0;
            while (start < text.size()) {
                auto end = text.find('\n', start);
                auto line = text.substr(start, end == std::string::npos ? end : end - start);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (result.size() >= 2000) throw std::runtime_error("Lyrics exceed 2000 lines");
                result.push_back(line);
                if (end == std::string::npos) break;
                start = end + 1;
            }
        }
        return result;
    }
};
