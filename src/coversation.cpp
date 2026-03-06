#include "conversation.h"
#include <fstream>
#include <sstream>
#include <iostream>

void Conversation::addMessage(const std::string& role, const std::string& content) {
    history.push_back({role, content});
}

const std::vector<Message>& Conversation::getHistory() const {
    return history;
}

void Conversation::clear() {
    history.clear();
}

std::string readFile(const std::string& filepath) {
    std::ifstream file(filepath);

    // Check if file opened successfully
    if (!file.is_open()) {
        return "Error: Could not open file: " + filepath;
    }

    // Read entire file into a string
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}