#include "markdown_parser.h"
#include <sstream>

std::string MarkdownParser::strip(const std::string& input) {
    std::string result;
    std::istringstream stream(input);
    std::string line;

    while (std::getline(stream, line)) {
        // Remove ### ## # headings
        if (line.size() >= 4 && line.substr(0, 4) == "### ")
            line = line.substr(4);
        else if (line.size() >= 3 && line.substr(0, 3) == "## ")
            line = line.substr(3);
        else if (line.size() >= 2 && line.substr(0, 2) == "# ")
            line = line.substr(2);

        // Replace bullet points
        if (line.size() >= 2 && (line.substr(0, 2) == "* " || line.substr(0, 2) == "- "))
            line = "  • " + line.substr(2);

        // Process character by character
        std::string cleaned;
        size_t i = 0;
        while (i < line.size()) {
            // Handle **bold**
            if (i + 1 < line.size() && line[i] == '*' && line[i+1] == '*') {
                i += 2; // skip opening **
                while (i + 1 < line.size() && !(line[i] == '*' && line[i+1] == '*')) {
                    cleaned += line[i++];
                }
                if (i + 1 < line.size()) i += 2; // skip closing **
            }
            // Handle *italic* — but only if it has a closing *
            else if (line[i] == '*') {
                // Check if there's a closing * ahead
                size_t closing = line.find('*', i + 1);
                if (closing != std::string::npos) {
                    i++; // skip opening *
                    while (i < line.size() && line[i] != '*') {
                        cleaned += line[i++];
                    }
                    if (i < line.size()) i++; // skip closing *
                } else {
                    // No closing * found — just remove the lone *
                    i++;
                }
            }
            // Handle `code`
            else if (line[i] == '`') {
                // Skip triple backticks ```
                if (i + 2 < line.size() && line[i+1] == '`' && line[i+2] == '`') {
                    i += 3;
                    while (i + 2 < line.size() && !(line[i] == '`' && line[i+1] == '`' && line[i+2] == '`')) {
                        cleaned += line[i++];
                    }
                    if (i + 2 < line.size()) i += 3;
                } else {
                    i++; // skip opening `
                    while (i < line.size() && line[i] != '`') {
                        cleaned += line[i++];
                    }
                    if (i < line.size()) i++; // skip closing `
                }
            }
            // Handle backslash escapes
            else if (line[i] == '\\' && i + 1 < line.size()) {
                i++; // skip backslash
                cleaned += line[i++];
            }
            else {
                cleaned += line[i++];
            }
        }   

        result += cleaned + "\n";
    }

    // Remove trailing newline
    if (!result.empty() && result.back() == '\n')
        result.pop_back();

    return result;
}