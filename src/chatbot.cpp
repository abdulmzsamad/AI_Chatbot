#include "chatbot.h"
#include <iostream>

void Chatbot::run() {
    printWelcome();

    std::string input;
    while (true) {
        std::cout << "You: ";
        std::getline(std::cin, input);

        if (input.empty()) continue;

        if (input == "quit") {
            std::cout << "Goodbye!\n";
            break;
        }

        if (input == "clear") {
            conv.clear();
            std::cout << "Chat history cleared.\n\n";
            continue;
        }

        if (input.substr(0, 5) == "read ") {
            handleFileCommand(input.substr(5));
            continue;
        }

        handleMessage(input);
    }
}

void Chatbot::printWelcome() {
    std::cout << "==============================\n";
    std::cout << "   AI Chatbot (Gemini 2.5)\n";
    std::cout << "==============================\n";
    std::cout << "Commands:\n";
    std::cout << "  'quit'            - exit\n";
    std::cout << "  'read <filepath>' - load a file\n";
    std::cout << "  'clear'           - clear chat history\n\n";
}

void Chatbot::handleFileCommand(const std::string& filepath) {
    std::string reply = FileReader::handleReadCommand(filepath, conv);

    if (reply.substr(0, 5) == "Error") {
        std::cout << reply << "\n\n";
        return;
    }

    sendAndPrint(reply);
}

void Chatbot::handleMessage(const std::string& input) {
    conv.addMessage("user", input);
    std::string reply = ApiClient::send(conv.getHistory());
    conv.addMessage("assistant", reply);
    sendAndPrint(reply);
}

void Chatbot::sendAndPrint(const std::string& reply) {
    std::cout << "Bot: thinking...\r";
    std::cout.flush();
    std::cout << "Bot: " << reply << "\n\n";
}