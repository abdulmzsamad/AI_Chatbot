#include "markdown_parser.h"
#include <sstream>

std::string MarkdownParser::strip(const std::string& input) {
    std::string result;
    std::istringstream stream(input);
    std::string line;

    while (std::getline(stream, line)) {
        // Remove ### headings
        if (line.size() >= 4 && line.substr(0, 4) == "### ") {
            line = line.substr(4);
        }
        // Remove ## headings
        else if (line.size() >= 3 && line.substr(0, 3) == "## ") {
            line = line.substr(3);
        }
        // Remove # headings
        else if (line.size() >= 2 && line.substr(0, 2) == "# ") {
            line = line.substr(2);
        }

        // Replace bullet points (* or -) with clean bullet
        if (line.size() >= 2 && (line.substr(0, 2) == "* " || line.substr(0, 2) == "- ")) {
            line = "  • " + line.substr(2);
        }

        // Remove **bold** and *italic* — handle both together
        std::string cleaned;
        size_t i = 0;
        while (i < line.size()) {
            // Check for **bold**
            if (i + 1 < line.size() && line[i] == '*' && line[i + 1] == '*') {
                i += 2; // skip opening **
                while (i + 1 < line.size() && !(line[i] == '*' && line[i + 1] == '*')) {
                    cleaned += line[i++];
                }
                i += 2; // skip closing **
            }
            // Check for *italic*
            else if (line[i] == '*') {
                i++; // skip opening *
                while (i < line.size() && line[i] != '*') {
                    cleaned += line[i++];
                }
                i++; // skip closing *
            }
            // Check for `code`
            else if (line[i] == '`') {
                i++; // skip opening `
                while (i < line.size() && line[i] != '`') {
                    cleaned += line[i++];
                }
                i++; // skip closing `
            }
            // Remove backslash escapes like \( \) \[ \] \* \_
            else if (line[i] == '\\' && i + 1 < line.size()) {
                i++; // skip backslash, keep next character
                cleaned += line[i++];
            }
            else {
                cleaned += line[i++];
            }
        }

        result += cleaned + "\n";
    }

    return result;
}