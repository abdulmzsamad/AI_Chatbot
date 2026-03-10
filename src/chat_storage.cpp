#include "chat_storage.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <windows.h>
#include <direct.h>
#include <chrono>
#include <algorithm>

static std::string escapeJson(const std::string& input) {
    std::string output;
    for (char c : input) {
        if (c == '"')       output += "\\\"";
        else if (c == '\\') output += "\\\\";
        else if (c == '\n') output += "\\n";
        else if (c == '\r') output += "\\r";
        else if (c == '\t') output += "\\t";
        else                output += c;
    }
    return output;
}

static std::string unescapeJson(const std::string& input) {
    std::string output;
    for (size_t i = 0; i < input.size(); i++) {
        if (input[i] == '\\' && i + 1 < input.size()) {
            char next = input[i + 1];
            if (next == '"')       { output += '"';  i++; }
            else if (next == '\\') { output += '\\'; i++; }
            else if (next == 'n')  { output += '\n'; i++; }
            else if (next == 'r')  { output += '\r'; i++; }
            else if (next == 't')  { output += '\t'; i++; }
            else                   { output += input[i]; }
        } else {
            output += input[i];
        }
    }
    return output;
}

std::string ChatStorage::generateId() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    return "chat_" + std::to_string(ms);
}

void ChatStorage::save(const std::string& sessionId,
                       const std::string& title,
                       const std::vector<Message>& history) {
    // Create chats/ directory if it doesn't exist
    _mkdir("chats");

    std::string filepath = "chats/" + sessionId + ".json";
    std::ofstream file(filepath);
    if (!file.is_open()) return;

    file << "{\n";
    file << "  \"id\": \"" << escapeJson(sessionId) << "\",\n";
    file << "  \"title\": \"" << escapeJson(title) << "\",\n";
    file << "  \"messages\": [\n";

    for (size_t i = 0; i < history.size(); i++) {
        file << "    {\"role\": \"" << escapeJson(history[i].role)
             << "\", \"content\": \"" << escapeJson(history[i].content) << "\"}";
        if (i + 1 < history.size()) file << ",";
        file << "\n";
    }

    file << "  ]\n";
    file << "}\n";
    file.close();
}

std::vector<Message> ChatStorage::load(const std::string& filepath) {
    std::vector<Message> messages;
    std::ifstream file(filepath);
    if (!file.is_open()) return messages;

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    // Simple JSON parser for our specific format
    size_t pos = 0;
    while ((pos = content.find("\"role\":", pos)) != std::string::npos) {
        // Parse role
        size_t roleStart = content.find("\"", pos + 7) + 1;
        size_t roleEnd   = content.find("\"", roleStart);
        std::string role = content.substr(roleStart, roleEnd - roleStart);

        // Parse content
        size_t contentPos   = content.find("\"content\":", roleEnd);
        size_t contentStart = content.find("\"", contentPos + 10) + 1;
        size_t contentEnd   = contentStart;
        while (contentEnd < content.size()) {
            if (content[contentEnd] == '\\') contentEnd += 2;
            else if (content[contentEnd] == '"') break;
            else contentEnd++;
        }
        std::string msgContent = unescapeJson(
            content.substr(contentStart, contentEnd - contentStart));

        messages.push_back({role, msgContent});
        pos = contentEnd;
    }

    return messages;
}

std::vector<ChatSession> ChatStorage::listSessions() {
    std::vector<ChatSession> sessions;

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA("chats\\*.json", &findData);
    if (hFind == INVALID_HANDLE_VALUE) return sessions;

    do {
        std::string filename = findData.cFileName;
        std::string filepath = "chats/" + filename;
        std::string id = filename.substr(0, filename.size() - 5);

        // Read title from file
        std::ifstream file(filepath);
        if (!file.is_open()) continue;
        std::string content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        file.close();

        std::string title = id;
        size_t titlePos = content.find("\"title\":");
        if (titlePos != std::string::npos) {
            size_t start = content.find("\"", titlePos + 8) + 1;
            size_t end   = start;
            while (end < content.size()) {
                if (content[end] == '\\') end += 2;
                else if (content[end] == '"') break;
                else end++;
            }
            title = unescapeJson(content.substr(start, end - start));
        }

        sessions.push_back({id, title, filepath});
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);

    // Sort by id descending (newest first)
    std::sort(sessions.begin(), sessions.end(),
        [](const ChatSession& a, const ChatSession& b) {
            return a.id > b.id;
        });

    return sessions;
}

void ChatStorage::deleteSession(const std::string& filepath) {
    DeleteFileA(filepath.c_str());
}