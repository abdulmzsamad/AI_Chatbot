# AI Chatbot

A conversational AI chatbot built in C++ using the Gemini API and ImGui. Supports multi-turn conversations, local AI models via Ollama, and persistent chat history.

## Features
- Multi-turn conversations with memory
- Real time streaming responses
- Animated particle background and chat bubble UI
- Switch between Gemini API and local Ollama models via dropdown
- Custom AI personality via system prompt
- Auto-saves every conversation to a local JSON file
- Sidebar to browse and reload past conversations
- Read and analyze files from your computer
- Built with C++17, CMake and ImGui

## Prerequisites
- C++ compiler (GCC 13+ recommended via MSYS2)
- CMake 3.15+
- Ninja build system
- libcurl (`pacman -S mingw-w64-ucrt-x86_64-curl` via MSYS2)
- Ollama (optional, for local models — download at [ollama.com](https://ollama.com))

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

> Chat history is saved locally to a `chats/` folder which is created automatically on first use.

## Usage

### Commands
| Command | Description |
|---|---|
| `read <filepath>` | Load a file for the bot to analyze |

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
│   ├── main.cpp              - Entry point, starts the application
│   ├── gui.cpp               - GUI rendering, chat bubbles, particle system, sidebar
│   ├── api_client.cpp        - Gemini and Ollama API communication
│   ├── conversation.cpp      - Chat history management and system prompt
│   ├── chat_storage.cpp      - Auto-saves chats to JSON, loads and lists them in sidebar
│   ├── file_reader.cpp       - File reading functionality
│   └── markdown_parser.cpp   - Strips markdown from API responses
├── include/
│   ├── gui.h
│   ├── api_client.h
│   ├── conversation.h
│   ├── chat_storage.h
│   ├── file_reader.h
│   └── markdown_parser.h
├── lib/
│   ├── imgui.h               - ImGui core (download separately)
│   ├── imgui.cpp
│   ├── imgui_*.cpp
│   └── backends/
│       ├── imgui_impl_opengl3.h
│       ├── imgui_impl_opengl3.cpp
│       ├── imgui_impl_win32.h
│       └── imgui_impl_win32.cpp
├── chats/                    - Auto-generated, stores saved conversations (gitignored)
├── .gitignore
├── CMakeLists.txt
└── README.md
```

## Roadmap
- [x] Refactor main.cpp into separate classes
- [x] Add GUI interface with ImGui
- [x] Add streaming responses
- [x] Add Ollama support for local models
- [x] Add markdown parser
- [x] Add custom AI personality via system prompt
- [x] Save and load conversation history with sidebar

## License
MIT