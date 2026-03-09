#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <winsock2.h>
#include <string>
#include <vector>
#include <functional>
#include <curl/curl.h>
#include "conversation.h"

class ApiClient {
public:
    static std::string send(const std::vector<Message>& history);
    static void sendStreaming(const std::vector<Message>& history,
                              std::function<void(const std::string&)> onChunk,
                              std::function<void()> onDone);

    // Call once at startup to initialize persistent connection
    static void init();
    static void cleanup();

private:
    static CURL* curlHandle; // persistent handle
};

#endif