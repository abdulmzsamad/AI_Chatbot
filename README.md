# AI Chatbot

A conversational AI chatbot built in C++ using the Gemini API. Supports multi-turn 
conversations and file reading.

## Features
- Multi-turn conversations with memory
- Read and analyze files from your computer
- Built with C++17 and CMake

## Prerequisites
- C++ compiler (GCC 13+ recommended)
- CMake 3.15+
- Ninja build system
- libcurl (`pacman -S mingw-w64-ucrt-x86_64-curl` via MSYS2)

## Setup

### 1. Clone the repository
```bash
git clone https://github.com/abdulmzsamad/AI_Chatbot.git
cd AI_Chatbot
```

### 2. Set your API key
Get a free API key from [Google AI Studio](https://aistudio.google.com)

**Windows:**
Add `GEMINI_API_KEY` to your system environment variables

**Linux/Mac:**
```bash
export GEMINI_API_KEY=your_key_here
```

### 3. Build
```bash
cmake -S . -B build -G "Ninja"
cmake --build build
```

### 4. Run
```bash
.\build\chatbot.exe
```

### 5. Download json.hpp (optional)
Download [json.hpp](https://github.com/nlohmann/json/releases/latest) and place it in the `lib/` folder for JSON parsing support.

## Usage

### Commands
| Command | Description |
|---|---|
| `read <filepath>` | Load a file for the bot to analyze |
| `clear` | Clear chat history |
| `quit` | Exit the chatbot |

### Example
```
You: hello
Bot: Hi! How can I help you today?

You: read C:/Users/YourName/Desktop/notes.txt
Bot: I have read the file...

You: summarize it
Bot: The file contains...
```

## Project Structure
```
ai-chatbot/
├── src/
│   ├── main.cpp          - Entry point, starts the chatbot
│   ├── chatbot.cpp       - Chat loop and command handling
│   ├── api_client.cpp    - Gemini API communication
│   ├── conversation.cpp  - Chat history management
│   └── file_reader.cpp   - File reading functionality
├── include/
│   ├── chatbot.h
│   ├── api_client.h
│   ├── conversation.h
│   └── file_reader.h
└── CMakeLists.txt
```

## Roadmap
- [x] Refactor main.cpp into separate classes
- [x] Add GUI interface
- [ ] Give the chatbot its own personality
- [ ] Save and load conversation history

## License
MIT