#ifndef CHAT_STORAGE_H
#define CHAT_STORAGE_H

#include <string>
#include <vector>
#include "conversation.h"

struct ChatSession {
    std::string id;        // unique filename without extension
    std::string title;     // first user message, truncated
    std::string filepath;  // full path to json file
};

class ChatStorage {
public:
    // Save current conversation to chats/ folder
    static void save(const std::string& sessionId,
                     const std::string& title,
                     const std::vector<Message>& history);

    // Load a conversation from file
    static std::vector<Message> load(const std::string& filepath);

    // List all saved chat sessions
    static std::vector<ChatSession> listSessions();

    // Delete a session
    static void deleteSession(const std::string& filepath);

    // Generate a unique session ID based on timestamp
    static std::string generateId();
};

#endif