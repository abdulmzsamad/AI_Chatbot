#include "api_client.h"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>

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

std::string ApiClient::send(const std::vector<Message>& history) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return "Error: Failed to initialize CURL";
    }

    // Build Gemini JSON
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

    if (res != CURLE_OK) {
        return "Error: " + std::string(curl_easy_strerror(res));
    }

    // Parse Gemini response - extract text from candidates[0].content.parts[0].text
    size_t pos = response.find("\"text\":");
    if (pos != std::string::npos) {
        size_t start = response.find("\"", pos + 7) + 1;
        size_t end = start;
        while (end < response.size()) {
            if (response[end] == '\\') {
                end += 2; // skip escaped character
            } else if (response[end] == '"') {
                break;
            } else {
                end++;
            }
        }
        std::string result = response.substr(start, end - start);
        // Unescape \n
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

    std::cout << "Raw response: " << response << std::endl;
    return "Error: Could not parse response";
}