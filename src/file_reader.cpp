    #include "file_reader.h"
    #include "api_client.h"
    #include <fstream>
    #include <sstream>
    #include <iostream>

    std::string FileReader::readFile(const std::string& filepath) {
        std::ifstream file(filepath);

        if (!file.is_open()) {
            return "Error: Could not open file: " + filepath;
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::string FileReader::createFileMessage(const std::string& filepath, const std::string& fileContent) {
        return "I'm sharing a file with you called '" 
            + filepath + "'. Here is its content:\n\n" 
            + fileContent 
            + "\n\nPlease confirm you have read it and wait for my question.";
    }

    std::string FileReader::handleReadCommand(const std::string& filepath, Conversation& conv) {
        std::string fileContent = readFile(filepath);

        if (fileContent.substr(0, 5) == "Error") {
            return fileContent;
        }

        std::string message = createFileMessage(filepath, fileContent);
        conv.addMessage("user", message);

        std::cout << "Bot: thinking...\r";
        std::cout.flush();

        std::string reply = ApiClient::send(conv);
        conv.addMessage("assistant", reply);

        return reply;
    }
