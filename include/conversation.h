#pragma once
#include <string>
#include <vector>

struct Message {
    std::string role;
    std::string content;
};

class Conversation {
public:
    void addMessage(const std::string& role, const std::string& content);
    const std::vector<Message>& getHistory() const;
    void clear();
private:
    std::vector<Message> history;
};  