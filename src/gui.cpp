#include "gui.h"
#include "api_client.h"
#include "markdown_parser.h"
#include "chat_storage.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_opengl3.h"
#include <windows.h>
#include <GL/gl.h>
#include <thread>
#include <sstream>
#include <algorithm>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <cctype>

// ─── Color Palette ─────────────────────────────────────────────────────────────
static const ImVec4 COL_BG          = ImVec4(0.12f, 0.12f, 0.15f, 1.0f);
static const ImVec4 COL_HEADER      = ImVec4(0.08f, 0.08f, 0.11f, 1.0f);
static const ImVec4 COL_USER_BUBBLE = ImVec4(0.25f, 0.15f, 0.45f, 1.0f);
static const ImVec4 COL_BOT_BUBBLE  = ImVec4(0.10f, 0.25f, 0.30f, 1.0f);
static const ImVec4 COL_USER_TEXT   = ImVec4(0.90f, 0.80f, 1.00f, 1.0f);
static const ImVec4 COL_BOT_TEXT    = ImVec4(0.80f, 1.00f, 0.95f, 1.0f);
static const ImVec4 COL_INPUT_BG    = ImVec4(0.18f, 0.18f, 0.22f, 1.0f);
static const ImVec4 COL_SEND_BTN    = ImVec4(0.45f, 0.20f, 0.70f, 1.0f);
static const ImVec4 COL_SEND_HOVER  = ImVec4(0.55f, 0.30f, 0.80f, 1.0f);
static const ImVec4 COL_SEND_ACTIVE = ImVec4(0.35f, 0.10f, 0.60f, 1.0f);
static const ImVec4 COL_ACCENT      = ImVec4(0.30f, 0.80f, 0.70f, 1.0f);
static const ImVec4 COL_THINKING    = ImVec4(0.50f, 0.50f, 0.60f, 1.0f);

// ─── Particle System ───────────────────────────────────────────────────────────
struct Particle {
    float x, y;
    float vx, vy;
    float radius;
    float alpha;
    float alphaSpeed;
};

static std::vector<Particle> particles;
static bool particlesInitialized = false;

static void initParticles(float width, float height) {
    particles.clear();
    for (int i = 0; i < 60; i++) {
        Particle p;
        p.x          = (float)(rand() % (int)width);
        p.y          = (float)(rand() % (int)height);
        p.vx         = ((float)(rand() % 100) - 50.0f) / 500.0f;
        p.vy         = ((float)(rand() % 100) - 50.0f) / 500.0f;
        p.radius     = 1.0f + (float)(rand() % 3);
        p.alpha      = 0.1f + (float)(rand() % 40) / 100.0f;
        p.alphaSpeed = 0.001f + (float)(rand() % 10) / 5000.0f;
        particles.push_back(p);
    }
    particlesInitialized = true;
}

static void updateAndDrawParticles(ImDrawList* draw, ImVec2 winPos, float width, float height) {
    if (!particlesInitialized) initParticles(width, height);

    for (auto& p : particles) {
        p.x += p.vx;
        p.y += p.vy;
        p.alpha += p.alphaSpeed;
        if (p.alpha > 0.5f || p.alpha < 0.05f) p.alphaSpeed = -p.alphaSpeed;
        if (p.x < 0) p.x = width;
        if (p.x > width) p.x = 0;
        if (p.y < 0) p.y = height;
        if (p.y > height) p.y = 0;

        int idx = (int)(&p - &particles[0]);
        ImU32 color = (idx % 2 == 0)
            ? IM_COL32(140, 80, 220, (int)(p.alpha * 255))
            : IM_COL32(50, 180, 160, (int)(p.alpha * 255));

        draw->AddCircleFilled(ImVec2(winPos.x + p.x, winPos.y + p.y), p.radius, color);
    }
}

// ─── sendMessage ───────────────────────────────────────────────────────────────
void GUI::sendMessage(const std::string& input) {
    isWaiting  = true;
    isStreaming = true;
    conv.addMessage("user", input);

    {
        std::lock_guard<std::mutex> lock(streamMutex);
        messages.push_back("Bot: ");
    }

    std::thread([this]() {
        std::string fullReply;

        ApiClient::sendStreaming(
            conv,
            [this, &fullReply](const std::string& chunk) {
                if (chunk.find("RESOURCE_EXHAUSTED") != std::string::npos ||
                    chunk.find("quota") != std::string::npos) {
                    std::lock_guard<std::mutex> lock(streamMutex);
                    messages.back() = "Bot: Daily quota reached. Please try again tomorrow.";
                    return;
                }
                if (chunk.find("authentication_error") != std::string::npos) {
                    std::lock_guard<std::mutex> lock(streamMutex);
                    messages.back() = "Bot: Invalid API key. Please check your GEMINI_API_KEY.";
                    return;
                }
                fullReply += chunk;
                std::string strippedChunk = MarkdownParser::strip(chunk);
                std::lock_guard<std::mutex> lock(streamMutex);
                messages.back() += strippedChunk;
            },
            [this, &fullReply]() {
                std::string formatted;
                for (size_t i = 0; i < fullReply.size(); i++) {
                    if (i > 0 && isdigit((unsigned char)fullReply[i])) {
                        size_t j = i;
                        while (j < fullReply.size() && isdigit((unsigned char)fullReply[j])) j++;
                        if (j < fullReply.size() && fullReply[j] == '.' && fullReply[i-1] != '\n') {
                            if (i > 1 && fullReply[i-2] != '\n') {
                                formatted += '\n';
                            }
                        }
                    }
                    formatted += fullReply[i];
                }

                std::string stripped = MarkdownParser::strip(formatted);
                conv.addMessage("assistant", stripped);
                {
                    std::lock_guard<std::mutex> lock(streamMutex);
                    messages.back() = "Bot: " + stripped;
                }
                isStreaming = false;
                isWaiting   = false;
                autoSave();
            }
        );
    }).detach();
}

// ─── autoSave ──────────────────────────────────────────────────────────────────
void GUI::autoSave() {
    if (conv.getHistory().empty()) return;

    if (currentSessionId.empty()) {
        currentSessionId = ChatStorage::generateId();
    }

    std::string title = "New Chat";
    for (const auto& msg : conv.getHistory()) {
        if (msg.role == "user") {
            title = msg.content.size() > 40
                ? msg.content.substr(0, 40) + "..."
                : msg.content;
            break;
        }
    }

    ChatStorage::save(currentSessionId, title, conv.getHistory());
    sidebarNeedsRefresh = true;
}

// ─── startNewChat ──────────────────────────────────────────────────────────────
void GUI::startNewChat() {
    conv.clear();
    messages.clear();
    currentSessionId = "";
    inputBuf[0] = '\0';
}

// ─── loadChat ──────────────────────────────────────────────────────────────────
void GUI::loadChat(const ChatSession& session) {
    std::vector<Message> loaded = ChatStorage::load(session.filepath);
    if (loaded.empty()) return;

    conv.clear();
    messages.clear();
    currentSessionId = session.id;

    for (const auto& msg : loaded) {
        conv.addMessage(msg.role, msg.content);
        if (msg.role == "user")
            messages.push_back("You: " + msg.content);
        else
            messages.push_back("Bot: " + msg.content);
    }
}

// ─── run ───────────────────────────────────────────────────────────────────────
void GUI::run() {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L,
                      GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                      "ImGui Chatbot", NULL };
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindow(wc.lpszClassName, "AI Chatbot",
                             WS_OVERLAPPEDWINDOW, 100, 100, 900, 680,
                             NULL, NULL, wc.hInstance, NULL);

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

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 24.0f);

    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();

    style.Colors[ImGuiCol_WindowBg]             = COL_BG;
    style.Colors[ImGuiCol_ChildBg]              = COL_BG;
    style.Colors[ImGuiCol_FrameBg]              = COL_INPUT_BG;
    style.Colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.22f, 0.22f, 0.28f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive]        = ImVec4(0.25f, 0.25f, 0.32f, 1.0f);
    style.Colors[ImGuiCol_Button]               = COL_SEND_BTN;
    style.Colors[ImGuiCol_ButtonHovered]        = COL_SEND_HOVER;
    style.Colors[ImGuiCol_ButtonActive]         = COL_SEND_ACTIVE;
    style.Colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.35f, 0.20f, 0.55f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.45f, 0.30f, 0.65f, 1.0f);
    style.Colors[ImGuiCol_Separator]            = ImVec4(0.20f, 0.20f, 0.26f, 1.0f);

    style.WindowRounding    = 0.0f;
    style.FrameRounding     = 8.0f;
    style.GrabRounding      = 8.0f;
    style.ScrollbarSize     = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.FramePadding      = ImVec2(12.0f, 10.0f);
    style.ItemSpacing       = ImVec2(10.0f, 8.0f);
    style.WindowPadding     = ImVec2(0.0f, 0.0f);

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL3_Init("#version 130");

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    srand((unsigned int)time(NULL));

    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("##main", nullptr,
                     ImGuiWindowFlags_NoTitleBar  |
                     ImGuiWindowFlags_NoResize    |
                     ImGuiWindowFlags_NoMove      |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

        ImDrawList* bgDraw = ImGui::GetWindowDrawList();
        updateAndDrawParticles(bgDraw, ImGui::GetWindowPos(),
                               io.DisplaySize.x, io.DisplaySize.y);

        if (showSidebar) renderSidebar();
        renderHeader();
        renderChatHistory();
        renderInputBox();

        ImGui::End();

        ImGui::Render();
        glClearColor(COL_BG.x, COL_BG.y, COL_BG.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SwapBuffers(hdc);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);
}

// ─── renderHeader ──────────────────────────────────────────────────────────────
void GUI::renderHeader() {
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, COL_HEADER);
    ImGui::BeginChild("##header", ImVec2(io.DisplaySize.x, 60.0f), false,
                      ImGuiWindowFlags_NoScrollbar);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();

    // Teal accent bar on left
    draw->AddRectFilled(ImVec2(p.x, p.y), ImVec2(p.x + 4, p.y + 60),
                        IM_COL32(50, 200, 170, 255));

    // Sidebar toggle button
    ImGui::SetCursorPos(ImVec2(12.0f, 13.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.0f,  0.0f,  0.0f,  0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.15f, 0.35f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.20f, 0.10f, 0.30f, 1.0f));
    if (ImGui::Button("=", ImVec2(34.0f, 34.0f))) {
        showSidebar    = !showSidebar;
        openModelPopup = false;
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    // Title
    ImGui::SetCursorPos(ImVec2(56.0f, 8.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
    ImGui::Text("AI Chatbot");
    ImGui::PopStyleColor();

    // Subtitle
    ImGui::SetCursorPos(ImVec2(56.0f, 34.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.6f, 1.0f));
    ImGui::Text("Powered by Gemini 2.5");
    ImGui::PopStyleColor();

    // Status dot
    ImVec2 dotPos   = ImVec2(p.x + io.DisplaySize.x - 24, p.y + 30);
    ImU32 dotColor  = isWaiting ? IM_COL32(120,120,130,255) : IM_COL32(50,200,120,255);
    ImU32 glowColor = isWaiting ? IM_COL32(120,120,130, 60) : IM_COL32(50,200,120, 60);
    draw->AddCircleFilled(dotPos, 7.0f,  dotColor);
    draw->AddCircleFilled(dotPos, 12.0f, glowColor);

    // Model dropdown button
    ImGui::SetCursorPos(ImVec2(io.DisplaySize.x - 255.0f, 13.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,
        ApiClient::useOllama ? ImVec4(0.10f,0.50f,0.30f,1.0f)
                             : ImVec4(0.25f,0.15f,0.45f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        ApiClient::useOllama ? ImVec4(0.15f,0.60f,0.35f,1.0f)
                             : ImVec4(0.35f,0.20f,0.55f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
        ApiClient::useOllama ? ImVec4(0.08f,0.40f,0.25f,1.0f)
                             : ImVec4(0.20f,0.10f,0.35f,1.0f));

    std::string buttonLabel = ApiClient::useOllama
        ? "Ollama: " + ApiClient::ollamaModel
        : "Gemini 2.5";
    while (buttonLabel.size() < 18) buttonLabel += " ";
    buttonLabel += " ▼";

    if (ImGui::Button(buttonLabel.c_str(), ImVec2(200.0f, 34.0f))) {
        openModelPopup = true;
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Separator line
    ImDrawList* mainDraw = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();
    mainDraw->AddLine(
        ImVec2(winPos.x, winPos.y + 60),
        ImVec2(winPos.x + io.DisplaySize.x, winPos.y + 60),
        IM_COL32(80, 40, 120, 255), 1.5f
    );

    // Open popup from main window context
    if (openModelPopup) {
        ImGui::OpenPopup("ModelSelectPopup");
        openModelPopup = false;
    }

    ImGui::SetNextWindowPos(ImVec2(winPos.x + io.DisplaySize.x - 255.0f, winPos.y + 61.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.12f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.40f, 0.20f, 0.60f, 1.0f));

    if (ImGui::BeginPopup("ModelSelectPopup")) {
        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
        ImGui::Text("Gemini");
        ImGui::PopStyleColor();
        ImGui::Separator();

        if (ImGui::Selectable("  Gemini 2.5 Flash", !ApiClient::useOllama))
            ApiClient::useOllama = false;

        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
        ImGui::Text("Ollama (Local)");
        ImGui::PopStyleColor();
        ImGui::Separator();

        static const char* ollamaModels[] = { "llama3.2" };
        for (const char* model : ollamaModels) {
            bool selected = ApiClient::useOllama && ApiClient::ollamaModel == model;
            std::string label = std::string("  ") + model;
            if (ImGui::Selectable(label.c_str(), selected)) {
                ApiClient::useOllama   = true;
                ApiClient::ollamaModel = model;
            }
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
}

// ─── renderChatHistory ─────────────────────────────────────────────────────────
void GUI::renderChatHistory() {
    ImGuiIO& io = ImGui::GetIO();
    float headerHeight  = 61.0f;
    float inputHeight   = 70.0f;
    float sidebarOffset = showSidebar ? 220.0f : 0.0f;
    float chatWidth     = io.DisplaySize.x - sidebarOffset;

    ImGui::SetCursorPos(ImVec2(sidebarOffset, headerHeight));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("ChatHistory",
                      ImVec2(chatWidth, io.DisplaySize.y - headerHeight - inputHeight),
                      false);

    ImDrawList* draw   = ImGui::GetWindowDrawList();
    ImVec2 winPos      = ImGui::GetWindowPos();
    float winWidth     = chatWidth;
    float padding      = 12.0f;
    float bubblePad    = 12.0f;
    float maxWidth     = winWidth * 0.65f;
    float scrollY      = ImGui::GetScrollY();

    ImGui::Dummy(ImVec2(0, 10.0f));

    for (const auto& msg : messages) {
        bool isUser = msg.size() >= 4 && msg.substr(0, 4) == "You:";
        std::string text = msg.size() > 5 ? msg.substr(5) : " ";

        ImVec2 singleLineSize = ImGui::CalcTextSize(text.c_str(), nullptr, false, -1.0f);
        float bubbleW = (singleLineSize.x + bubblePad * 2 <= maxWidth)
            ? singleLineSize.x + bubblePad * 2
            : maxWidth;

        float wrapW     = bubbleW - bubblePad * 2;
        ImVec2 textSize = ImGui::CalcTextSize(text.c_str(), nullptr, false, wrapW);
        float bubbleH   = textSize.y + bubblePad * 2;
        float cursorY   = ImGui::GetCursorPosY();
        float bubbleX   = isUser ? (winWidth - bubbleW - padding) : padding;

        ImVec2 bMin = ImVec2(winPos.x + bubbleX,            winPos.y + cursorY - scrollY);
        ImVec2 bMax = ImVec2(winPos.x + bubbleX + bubbleW,  winPos.y + cursorY - scrollY + bubbleH);

        draw->AddRectFilled(ImVec2(bMin.x+3,bMin.y+3), ImVec2(bMax.x+3,bMax.y+3),
                            IM_COL32(0,0,0,60), 10.0f);

        ImU32 bubbleColor = isUser ? IM_COL32(64,38,115,230) : IM_COL32(25,64,77,230);
        draw->AddRectFilled(bMin, bMax, bubbleColor, 10.0f);

        ImU32 borderColor = isUser ? IM_COL32(120,60,200,180) : IM_COL32(50,180,160,180);
        draw->AddRect(bMin, bMax, borderColor, 10.0f, 0, 1.5f);

        ImVec4 textColor = isUser
            ? ImVec4(COL_USER_TEXT.x, COL_USER_TEXT.y, COL_USER_TEXT.z, 1.0f)
            : ImVec4(COL_BOT_TEXT.x,  COL_BOT_TEXT.y,  COL_BOT_TEXT.z,  1.0f);

        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                      ImVec2(bMin.x + bubblePad, bMin.y + bubblePad),
                      ImGui::ColorConvertFloat4ToU32(textColor),
                      text.c_str(), nullptr, wrapW);

        ImGui::Dummy(ImVec2(winWidth, bubbleH + 8.0f));
    }

    // Animated waiting dots
    if (isWaiting) {
        float time      = (float)ImGui::GetTime();
        float dotRadius = 5.0f;
        float dotSpace  = 18.0f;
        float bounceH   = 6.0f;

        ImGui::Dummy(ImVec2(0, 5.0f));
        ImGui::SetCursorPosX(padding + bubblePad);

        ImDrawList* dotDraw = ImGui::GetWindowDrawList();
        ImVec2 dotStart = ImGui::GetCursorScreenPos();

        for (int d = 0; d < 3; d++) {
            float phase  = time * 4.0f - d * 0.4f;
            float bounce = sinf(phase) * bounceH;
            if (bounce < 0) bounce = 0;

            ImVec2 center = ImVec2(dotStart.x + d * dotSpace + dotRadius,
                                   dotStart.y + dotRadius - bounce);
            ImU32 dotColor = (d % 2 == 0)
                ? IM_COL32(140,80,220,220)
                : IM_COL32(50,180,160,220);
            dotDraw->AddCircleFilled(center, dotRadius, dotColor);
        }

        ImGui::Dummy(ImVec2(winWidth, dotRadius * 2 + bounceH + 8.0f));
    }

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f)
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ─── renderInputBox ────────────────────────────────────────────────────────────
void GUI::renderInputBox() {
    ImGuiIO& io = ImGui::GetIO();
    float inputAreaHeight = 70.0f;
    float inputY          = io.DisplaySize.y - inputAreaHeight;
    float sidebarOffset   = showSidebar ? 220.0f : 0.0f;
    float chatWidth       = io.DisplaySize.x - sidebarOffset;

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 winPos    = ImGui::GetWindowPos();

    draw->AddRectFilled(
        ImVec2(winPos.x + sidebarOffset, winPos.y + inputY - 1),
        ImVec2(winPos.x + io.DisplaySize.x, winPos.y + io.DisplaySize.y),
        IM_COL32(15, 15, 20, 255)
    );
    draw->AddLine(
        ImVec2(winPos.x + sidebarOffset, winPos.y + inputY - 1),
        ImVec2(winPos.x + io.DisplaySize.x, winPos.y + inputY - 1),
        IM_COL32(80, 40, 120, 255), 1.5f
    );

    ImGui::SetCursorPos(ImVec2(sidebarOffset + 15.0f, inputY + 14.0f));

    if (refocusInput) {
        ImGui::SetKeyboardFocusHere(0);
        refocusInput = false;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
    ImGui::SetNextItemWidth(chatWidth - 110.0f);
    bool entered = ImGui::InputText("##input", inputBuf, sizeof(inputBuf),
                                    ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopStyleVar();

    ImGui::SameLine(0, 10.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
    if (isWaiting) {
        ImGui::BeginDisabled();
        ImGui::Button("Send", ImVec2(75.0f, 0));
        ImGui::EndDisabled();
    } else {
        bool sendClicked = ImGui::Button("Send", ImVec2(75.0f, 0));
        if ((sendClicked || entered) && inputBuf[0] != '\0') {
            std::string userInput(inputBuf);
            messages.push_back("You: " + userInput);
            sendMessage(userInput);
            inputBuf[0] = '\0';
            refocusInput = true;
        }
    }
    ImGui::PopStyleVar();
}

// ─── renderSidebar ─────────────────────────────────────────────────────────────
void GUI::renderSidebar() {
    ImGuiIO& io = ImGui::GetIO();
    float sidebarWidth = 220.0f;
    float headerHeight = 61.0f;

    if (sidebarNeedsRefresh) {
        sidebarSessions     = ChatStorage::listSessions();
        sidebarNeedsRefresh = false;
    }

    ImGui::SetCursorPos(ImVec2(0, headerHeight));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.12f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    ImGui::BeginChild("##sidebar", ImVec2(sidebarWidth, io.DisplaySize.y - headerHeight), false);

    ImDrawList* draw  = ImGui::GetWindowDrawList();
    ImVec2 sidebarPos = ImGui::GetWindowPos();

    // Right border
    draw->AddLine(
        ImVec2(sidebarPos.x + sidebarWidth - 1, sidebarPos.y),
        ImVec2(sidebarPos.x + sidebarWidth - 1, sidebarPos.y + io.DisplaySize.y),
        IM_COL32(80, 40, 120, 255), 1.5f
    );

    // New Chat button — no SetCursorPos needed, padding handles it
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.25f, 0.15f, 0.45f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.20f, 0.55f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.20f, 0.10f, 0.35f, 1.0f));
    if (ImGui::Button("+ New Chat", ImVec2(sidebarWidth - 20.0f, 32.0f))) {
        startNewChat();
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::Spacing();

    // Section label
    ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
    ImGui::Text("Recent Chats");
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Separator
    ImVec2 sepPos = ImGui::GetCursorScreenPos();
    draw->AddLine(
        ImVec2(sepPos.x, sepPos.y),
        ImVec2(sepPos.x + sidebarWidth - 20.0f, sepPos.y),
        IM_COL32(80, 40, 120, 180), 1.0f
    );
    ImGui::Dummy(ImVec2(sidebarWidth - 20.0f, 4.0f));

    if (sidebarSessions.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.5f, 1.0f));
        ImGui::TextWrapped("No saved chats yet.");
        ImGui::PopStyleColor();
    }

    for (int i = 0; i < (int)sidebarSessions.size(); i++) {
        const auto& session = sidebarSessions[i];
        bool isActive = session.id == currentSessionId;

        ImGui::PushID(i);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        if (isActive) {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.10f, 0.40f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.15f, 0.45f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.20f, 0.10f, 0.40f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.0f,  0.0f,  0.0f,  0.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.10f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.20f, 0.10f, 0.35f, 1.0f));
        }

        std::string label = session.title;
        if (label.size() > 22) label = label.substr(0, 22) + "...";

        if (ImGui::Button(label.c_str(), ImVec2(sidebarWidth - 20.0f, 36.0f))) {
            loadChat(session);
        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        ImGui::PopID();

        ImGui::Spacing();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}