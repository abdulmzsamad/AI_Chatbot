#ifndef CHATBOT_H
#define CHATBOT_H

#include "conversation.h"
#include "file_reader.h"
#include "api_client.h"
#include <string>

class Chatbot {
public:
    void run();

private:
    Conversation conv;

    void printWelcome();
    void handleFileCommand(const std::string& filepath);
    void handleMessage(const std::string& input);
    void sendAndPrint(const std::string& reply);
};

#endif