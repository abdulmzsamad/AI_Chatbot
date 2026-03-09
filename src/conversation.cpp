#include "conversation.h"

void Conversation::addMessage(const std::string& role, const std::string& content) {
    history.push_back({role, content});
}

const std::vector<Message>& Conversation::getHistory() const {
    return history;
}

void Conversation::clear() {
    history.clear();
}