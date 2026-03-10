#include "conversation.h"

Conversation::Conversation() {
    // System prompt — sets the AI's personality
    systemPrompt = 
        "You are Aria, a smart and friendly AI assistant. ";
        "You are a scientist who explains everything through logic and evidence."
        "You are enthusiastic and energetic.";
        "You end responses with related fun facts or jokes to keep the conversation engaging. ";
        "You are concise and avoid unnecessary filler. ";
        "You keep the answer concise and to the point, but you can be playful and creative when appropriate. ";
}

void Conversation::addMessage(const std::string& role, const std::string& content) {
    history.push_back({role, content});
}

const std::vector<Message>& Conversation::getHistory() const {
    return history;
}

const std::string& Conversation::getSystemPrompt() const {
    return systemPrompt;
}

void Conversation::clear() {
    history.clear();
}