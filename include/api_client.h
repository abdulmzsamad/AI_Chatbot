#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <winsock2.h>
#include <curl/curl.h>
#include <string>
#include <vector>
#include <functional>
#include "conversation.h"

class ApiClient {
public:
    static std::string send(const Conversation& conv);
    static void sendStreaming(const Conversation& conv,
                              std::function<void(const std::string&)> onChunk,
                              std::function<void()> onDone);

    static bool useOllama;
    static std::string ollamaModel;

private:
    static void sendOllamaStreaming(const Conversation& conv,
                                    std::function<void(const std::string&)> onChunk,
                                    std::function<void()> onDone);
};

#endif