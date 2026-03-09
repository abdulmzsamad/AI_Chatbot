#ifndef MARKDOWN_PARSER_H
#define MARKDOWN_PARSER_H

#include <string>

class MarkdownParser {
public:
    static std::string strip(const std::string& input);
};

#endif