#ifndef FILE_READER_H
#define FILE_READER_H

#include <string>
#include "conversation.h"

class FileReader {
public:
    // Read file content and return it (or error message if failed)
    static std::string readFile(const std::string& filepath);

    // Create a message to send to the API with file content
    static std::string createFileMessage(const std::string& filepath, const std::string& fileContent);

    // Process the read command and return the bot's response
    static std::string handleReadCommand(const std::string& filepath, Conversation& conv);
};

#endif
