#include <iostream>
#include <string>
#include "api_client.h"
#include "conversation.h"
#include "file_reader.h"

int main() {
    Conversation conv;
    std::string input;

    std::cout << "==============================\n";
    std::cout << "   AI Chatbot (Gemini 2.5)\n";
    std::cout << "==============================\n";
    std::cout << "Commands:\n";
    std::cout << "  'quit'            - exit\n";
    std::cout << "  'read <filepath>' - load a file\n";
    std::cout << "  'clear'           - clear chat history\n\n";

    while (true) {
        std::cout << "You: ";
        std::getline(std::cin, input);

        if (input.empty()) continue;

        // Quit command
        if (input == "quit") {
            std::cout << "Goodbye!\n";
            break;
        }

        // Clear history command
        if (input == "clear") {
            conv.clear();
            std::cout << "Chat history cleared.\n\n";
            continue;
        }

        // Read file command — usage: read C:/path/to/file.txt
        if (input.substr(0, 5) == "read ") {
            std::string filepath = input.substr(5);
            std::string reply = FileReader::handleReadCommand(filepath, conv);

            if (reply.substr(0, 5) == "Error") {
                std::cout << reply << "\n\n";
                continue;
            }

            std::cout << "Bot: " << reply << "\n\n";
            continue;
        }

        // Normal message
        conv.addMessage("user", input);
        std::cout << "Bot: thinking...\r";
        std::cout.flush();

        std::string reply = ApiClient::send(conv.getHistory());
        conv.addMessage("assistant", reply);
        std::cout << "Bot: " << reply << "\n\n";
    }

    return 0;
}