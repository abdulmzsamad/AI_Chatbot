#include "gui.h"
#include "api_client.h"
#include "markdown_parser.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_opengl3.h"
#include <windows.h>
#include <GL/gl.h>
#include <thread>
#include <sstream>

void GUI::sendMessage(const std::string& input) {
    isWaiting = true;
    isStreaming = true;
    conv.addMessage("user", input);

    // Add empty bot message that we'll fill in as chunks arrive
    {
        std::lock_guard<std::mutex> lock(streamMutex);
        messages.push_back("Bot: ");
    }

    std::thread([this]() {
        std::string fullReply;

        ApiClient::sendStreaming(
            conv.getHistory(),
            // onChunk — called for each piece of text
            [this, &fullReply](const std::string& chunk) {
                std::string stripped = MarkdownParser::strip(chunk);
                fullReply += stripped;
                std::lock_guard<std::mutex> lock(streamMutex);
                messages.back() += stripped; // append to last message
            },
            // onDone — called when streaming is complete
            [this, &fullReply]() {
                conv.addMessage("assistant", fullReply);
                isStreaming = false;
                isWaiting = false;
            }
        );
    }).detach();
}

void GUI::run() {
    // Create Win32 window
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L,
                      GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                      "ImGui Chatbot", NULL };
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindow(wc.lpszClassName, "AI Chatbot",
                             WS_OVERLAPPEDWINDOW, 100, 100, 900, 650,
                             NULL, NULL, wc.hInstance, NULL);

    // Setup OpenGL
    HDC hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR), 1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        24, 8, 0, PFD_MAIN_PLANE, 0, 0, 0, 0
    };
    int pf = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, pf, &pfd);
    HGLRC hglrc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, hglrc);

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Font size
    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 24.0f);

    // Style
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    style.Colors[ImGuiCol_WindowBg]       = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
    style.Colors[ImGuiCol_FrameBg]        = ImVec4(0.14f, 0.14f, 0.18f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.26f, 1.0f);
    style.Colors[ImGuiCol_Button]         = ImVec4(0.20f, 0.50f, 0.80f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered]  = ImVec4(0.30f, 0.60f, 0.90f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive]   = ImVec4(0.10f, 0.40f, 0.70f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarBg]    = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrab]  = ImVec4(0.30f, 0.30f, 0.40f, 1.0f);
    style.Colors[ImGuiCol_Separator]      = ImVec4(0.20f, 0.20f, 0.26f, 1.0f);
    style.WindowRounding  = 8.0f;
    style.FrameRounding   = 6.0f;
    style.GrabRounding    = 6.0f;
    style.ScrollbarSize   = 10.0f;
    style.FramePadding    = ImVec2(10.0f, 8.0f);
    style.ItemSpacing     = ImVec2(10.0f, 8.0f);
    style.WindowPadding   = ImVec2(15.0f, 15.0f);

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL3_Init("#version 130");

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    // Warm up connection in background
    std::thread([]() {
        ApiClient::sendStreaming(
            {{"user", "hi"}},
            [](const std::string&) {},
            []() {}
        );
    }).detach();

    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;


        // Start new ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("AI Chatbot", nullptr,
                     ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove);

        renderChatHistory();
        ImGui::Separator();
        renderInputBox();

        ImGui::End();

        ImGui::Render();
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SwapBuffers(hdc);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);
}

void GUI::renderChatHistory() {
    float inputHeight = 55.0f;
    ImGui::BeginChild("ChatHistory",
                      ImVec2(0, -inputHeight),
                      false,
                      ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& msg : messages) {
        bool isUser = msg.substr(0, 4) == "You:";

        // Set color
        if (isUser) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        }

        // Calculate indent based on prefix width
        // "You: " or "Bot: " prefix
        float indentWidth = ImGui::CalcTextSize("You: ").x;

        // Print each line with proper indentation
        std::istringstream stream(msg);
        std::string line;
        bool firstLine = true;

        while (std::getline(stream, line)) {
            if (firstLine) {
                // First line prints normally
                ImGui::TextWrapped("%s", line.c_str());
                firstLine = false;
            } else {
                // Subsequent lines get indented to match
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indentWidth);
                ImGui::TextWrapped("%s", line.empty() ? " " : line.c_str());
            }
        }

        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    // Show thinking indicator
    // Show streaming indicator or thinking
    if (isWaiting && !isStreaming) {
        // Animated dots — cycles through ".", "..", "..."
        float time = (float)ImGui::GetTime();
        int dots = (int)(time * 2.0f) % 3 + 1;
        std::string indicator = "Bot is thinking";
        for (int i = 0; i < dots; i++) indicator += ".";

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::Text("%s", indicator.c_str());
        ImGui::PopStyleColor();
    }

    // Auto scroll to bottom
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

void GUI::renderInputBox() {
    ImGui::SetNextItemWidth(-80);
    bool entered = ImGui::InputText("##input", inputBuf, sizeof(inputBuf),
                                    ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();

    // Disable send button while waiting
    if (isWaiting) {
        ImGui::BeginDisabled();
        ImGui::Button("Send");
        ImGui::EndDisabled();
    } else {
        if ((ImGui::Button("Send") || entered) && inputBuf[0] != '\0') {
            std::string userInput(inputBuf);
            messages.push_back("You: " + userInput);
            sendMessage(userInput);
            inputBuf[0] = '\0';
        }
    }
}