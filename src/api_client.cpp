#include "api_client.h"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <functional>

// ─── Static member definitions ─────────────────────────────────────────────────
bool ApiClient::useOllama = false;
std::string ApiClient::ollamaModel = "llama3.2";

// ─── Helpers ───────────────────────────────────────────────────────────────────
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    output->append((char*)contents, size * nmemb);
    return size * nmemb;
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

// ─── Gemini streaming callback ─────────────────────────────────────────────────
struct StreamData {
    std::function<void(const std::string&)> onChunk;
    std::string buffer;
};

static size_t StreamCallback(void* contents, size_t size, size_t nmemb, StreamData* data) {
    size_t totalSize = size * nmemb;
    std::string chunk((char*)contents, totalSize);
    data->buffer += chunk;

    if (data->buffer.find("RESOURCE_EXHAUSTED") != std::string::npos) {
        data->onChunk("RESOURCE_EXHAUSTED");
        data->buffer.clear();
        return totalSize;
    }
    if (data->buffer.find("authentication_error") != std::string::npos) {
        data->onChunk("authentication_error");
        data->buffer.clear();
        return totalSize;
    }

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
                    if (json[end] == '\\' && end + 1 < json.size()) end += 2;
                    else if (json[end] == '"') break;
                    else end++;
                }
                std::string text = json.substr(start, end - start);
                std::string unescaped;
                for (size_t i = 0; i < text.size(); i++) {
                    if (text[i] == '\\' && i + 1 < text.size() && text[i+1] == 'n') {
                        unescaped += '\n'; i++;
                    } else {
                        unescaped += text[i];
                    }
                }
                if (!unescaped.empty()) data->onChunk(unescaped);
            }
        }
    }
    return totalSize;
}

// ─── Ollama streaming callback ─────────────────────────────────────────────────
struct OllamaStreamData {
    std::function<void(const std::string&)> onChunk;
    std::string buffer;
};

static size_t OllamaStreamCallback(void* contents, size_t size, size_t nmemb, OllamaStreamData* data) {
    size_t totalSize = size * nmemb;
    std::string chunk((char*)contents, totalSize);
    data->buffer += chunk;

    size_t pos;
    while ((pos = data->buffer.find('\n')) != std::string::npos) {
        std::string line = data->buffer.substr(0, pos);
        data->buffer = data->buffer.substr(pos + 1);

        // Chat API uses "content" field instead of "response"
        size_t contentPos = line.find("\"content\":\"");
        if (contentPos != std::string::npos) {
            size_t start = contentPos + 11;
            size_t end = start;
            while (end < line.size()) {
                if (line[end] == '\\' && end + 1 < line.size()) end += 2;
                else if (line[end] == '"') break;
                else end++;
            }
            std::string text = line.substr(start, end - start);
            std::string unescaped;
            for (size_t i = 0; i < text.size(); i++) {
                if (text[i] == '\\' && i + 1 < text.size() && text[i+1] == 'n') {
                    unescaped += '\n'; i++;
                } else {
                    unescaped += text[i];
                }
            }
            if (!unescaped.empty()) {
                data->onChunk(unescaped);
            }
        }
    }
    return totalSize;
}

// ─── Gemini send (non-streaming) ───────────────────────────────────────────────
std::string ApiClient::send(const std::vector<Message>& history) {
    CURL* curl = curl_easy_init();
    if (!curl) return "Error: Failed to initialize CURL";

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
    if (!apiKeyEnv) return "Error: GEMINI_API_KEY environment variable not set";
    std::string apiKey = std::string(apiKeyEnv);
    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=" + apiKey;

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return "Error: " + std::string(curl_easy_strerror(res));
    if (response.find("RESOURCE_EXHAUSTED") != std::string::npos)
        return "Error: Daily quota exceeded. Please try again tomorrow.";
    if (response.find("authentication_error") != std::string::npos)
        return "Error: Invalid API key.";

    size_t pos = response.find("\"text\":");
    if (pos != std::string::npos) {
        size_t start = response.find("\"", pos + 7) + 1;
        size_t end = start;
        while (end < response.size()) {
            if (response[end] == '\\') end += 2;
            else if (response[end] == '"') break;
            else end++;
        }
        std::string result = response.substr(start, end - start);
        std::string unescaped;
        for (size_t i = 0; i < result.size(); i++) {
            if (result[i] == '\\' && i + 1 < result.size() && result[i+1] == 'n') {
                unescaped += '\n'; i++;
            } else {
                unescaped += result[i];
            }
        }
        return unescaped;
    }
    return "Error: Could not parse response";
}

// ─── Gemini streaming ──────────────────────────────────────────────────────────
void ApiClient::sendStreaming(const std::vector<Message>& history,
                               std::function<void(const std::string&)> onChunk,
                               std::function<void()> onDone) {
    if (useOllama) {
        sendOllamaStreaming(history, onChunk, onDone);
        return;
    }

    CURL* curl = curl_easy_init();
    if (!curl) { onDone(); return; }

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
    if (!apiKeyEnv) { onDone(); return; }
    std::string apiKey = std::string(apiKeyEnv);
    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:streamGenerateContent?alt=sse&key=" + apiKey;

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
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    onDone();
}

// ─── Ollama streaming ──────────────────────────────────────────────────────────
void ApiClient::sendOllamaStreaming(const std::vector<Message>& history,
                                     std::function<void(const std::string&)> onChunk,
                                     std::function<void()> onDone) {
    CURL* curl = curl_easy_init();
    if (!curl) { onDone(); return; }

    // Use Ollama chat API instead of generate API
    // This handles conversation history properly
    std::ostringstream json;
    json << "{\"model\":\"" << ollamaModel << "\",\"messages\":[";
    for (size_t i = 0; i < history.size(); i++) {
        if (i > 0) json << ",";
        std::string role = history[i].role == "assistant" ? "assistant" : "user";
        json << "{\"role\":\"" << role << "\","
             << "\"content\":\"" << escapeJson(history[i].content) << "\"}";
    }
    json << "],\"stream\":true}";

    std::string jsonStr = json.str();

    // Use chat endpoint instead of generate endpoint
    std::string url = "http://localhost:11434/api/chat";

    OllamaStreamData streamData;
    streamData.onChunk = onChunk;

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, OllamaStreamCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &streamData);

    curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    onDone();
}