#pragma once
#include <string>
#include <vector>
#include "conversation.h"

class ApiClient {
public:
    static std::string send(const std::vector<Message>& history);
};