#include "api_client.h"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <functional>

CURL* ApiClient::curlHandle = nullptr;

void ApiClient::init() {
    curl_global_init(CURL_GLOBAL_ALL);
    curlHandle = curl_easy_init();
}

void ApiClient::cleanup() {
    if (curlHandle) {
        curl_easy_cleanup(curlHandle);
        curlHandle = nullptr;
    }
    curl_global_cleanup();
}

// ─── Original response callback ───────────────────────────────────────────────
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    output->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// ─── Streaming callback data ───────────────────────────────────────────────────
struct StreamData {
    std::function<void(const std::string&)> onChunk;
    std::string buffer;
};

static size_t StreamCallback(void* contents, size_t size, size_t nmemb, StreamData* data) {
    size_t totalSize = size * nmemb;
    std::string chunk((char*)contents, totalSize);
    data->buffer += chunk;

    size_t pos;
    while ((pos = data->buffer.find('\n')) != std::string::npos) {
        std::string line = data->buffer.substr(0, pos);
        data->buffer = data->buffer.substr(pos + 1);

        if (line.size() >= 6 && line.substr(0, 6) == "data: ") {
            std::string json = line.substr(6);

            size_t textPos = json.find("\"text\": \"");
            if (textPos != std::string::npos) {
                size_t start = textPos + 9;
                size_t end = start;
                while (end < json.size()) {
                    if (json[end] == '\\' && end + 1 < json.size()) {
                        end += 2;
                    } else if (json[end] == '"') {
                        break;
                    } else {
                        end++;
                    }
                }
                std::string text = json.substr(start, end - start);

                // Unescape \n
                std::string unescaped;
                for (size_t i = 0; i < text.size(); i++) {
                    if (text[i] == '\\' && i + 1 < text.size() && text[i+1] == 'n') {
                        unescaped += '\n';
                        i++;
                    } else {
                        unescaped += text[i];
                    }
                }

                if (!unescaped.empty()) {
                    data->onChunk(unescaped);
                }
            }
        }
    }
    return totalSize;
}

static std::string escapeJson(const std::string& input) {
    std::string output;
    for (char c : input) {
        if (c == '"')       output += "\\\"";
        else if (c == '\\') output += "\\\\";
        else if (c == '\n') output += "\\n";
        else if (c == '\r') output += "\\r";
        else if (c == '\t') output += "\\t";
        else                output += c;
    }
    return output;
}

// ─── Original send ─────────────────────────────────────────────────────────────
std::string ApiClient::send(const std::vector<Message>& history) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return "Error: Failed to initialize CURL";
    }

    std::ostringstream json;
    json << R"({"contents":[)";
    for (size_t i = 0; i < history.size(); i++) {
        if (i > 0) json << ",";
        std::string role = history[i].role == "assistant" ? "model" : "user";
        json << "{\"role\":\"" << role
             << "\",\"parts\":[{\"text\":\"" << escapeJson(history[i].content) << "\"}]}";
    }
    json << "]}";

    std::string jsonStr = json.str();
    std::string response;

    const char* apiKeyEnv = std::getenv("GEMINI_API_KEY");
    if (!apiKeyEnv) {
        return "Error: GEMINI_API_KEY environment variable not set";
    }
    std::string apiKey = std::string(apiKeyEnv);
    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=" + apiKey;

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);        // disable Nagle's algorithm
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);     // fail fast if no connection
    curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 60L); // cache DNS for 60 seconds
    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return "Error: " + std::string(curl_easy_strerror(res));
    }

    if (response.find("RESOURCE_EXHAUSTED") != std::string::npos) {
        return "Error: Daily quota exceeded. Please wait and try again tomorrow.";
    }
    if (response.find("authentication_error") != std::string::npos) {
        return "Error: Invalid API key.";
    }

    size_t pos = response.find("\"text\":");
    if (pos != std::string::npos) {
        size_t start = response.find("\"", pos + 7) + 1;
        size_t end = start;
        while (end < response.size()) {
            if (response[end] == '\\') {
                end += 2;
            } else if (response[end] == '"') {
                break;
            } else {
                end++;
            }
        }
        std::string result = response.substr(start, end - start);
        std::string unescaped;
        for (size_t i = 0; i < result.size(); i++) {
            if (result[i] == '\\' && i + 1 < result.size() && result[i+1] == 'n') {
                unescaped += '\n';
                i++;
            } else {
                unescaped += result[i];
            }
        }
        return unescaped;
    }

    return "Error: Could not parse response";
}

// ─── Streaming send ────────────────────────────────────────────────────────────
void ApiClient::sendStreaming(const std::vector<Message>& history,
                               std::function<void(const std::string&)> onChunk,
                               std::function<void()> onDone) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        onDone();
        return;
    }

    std::ostringstream json;
    json << R"({"contents":[)";
    for (size_t i = 0; i < history.size(); i++) {
        if (i > 0) json << ",";
        std::string role = history[i].role == "assistant" ? "model" : "user";
        json << "{\"role\":\"" << role
             << "\",\"parts\":[{\"text\":\"" << escapeJson(history[i].content) << "\"}]}";
    }
    json << "]}";

    std::string jsonStr = json.str();

    const char* apiKeyEnv = std::getenv("GEMINI_API_KEY");
    if (!apiKeyEnv) {
        onDone();
        return;
    }
    std::string apiKey = std::string(apiKeyEnv);
    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:streamGenerateContent?alt=sse&key=" + apiKey;

    StreamData streamData;
    streamData.onChunk = onChunk;

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StreamCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &streamData);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    onDone();
}