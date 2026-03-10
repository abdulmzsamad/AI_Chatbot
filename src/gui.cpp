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
#include <algorithm>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>

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
    float x, y;       // position
    float vx, vy;     // velocity
    float radius;     // size
    float alpha;      // opacity
    float alphaSpeed; // fade speed
};

static std::vector<Particle> particles;
static bool particlesInitialized = false;

static void initParticles(float width, float height) {
    particles.clear();
    for (int i = 0; i < 60; i++) {
        Particle p;
        p.x          = (float)(rand() % (int)width);
        p.y          = (float)(rand() % (int)height);
        p.vx         = ((float)(rand() % 100) - 50.0f) / 500.0f; // slow drift
        p.vy         = ((float)(rand() % 100) - 50.0f) / 500.0f;
        p.radius     = 1.0f + (float)(rand() % 3);
        p.alpha      = 0.1f + (float)(rand() % 40) / 100.0f;
        p.alphaSpeed = 0.001f + (float)(rand() % 10) / 5000.0f;
        particles.push_back(p);
    }
    particlesInitialized = true;
}

static void updateAndDrawParticles(ImDrawList* draw, ImVec2 winPos, float width, float height) {
    if (!particlesInitialized) {
        initParticles(width, height);
    }

    for (auto& p : particles) {
        // Move particle
        p.x += p.vx;
        p.y += p.vy;

        // Fade in and out
        p.alpha += p.alphaSpeed;
        if (p.alpha > 0.5f || p.alpha < 0.05f) {
            p.alphaSpeed = -p.alphaSpeed;
        }

        // Wrap around edges
        if (p.x < 0) p.x = width;
        if (p.x > width) p.x = 0;
        if (p.y < 0) p.y = height;
        if (p.y > height) p.y = 0;

        // Alternate between purple and teal particles
        int idx = (int)(&p - &particles[0]);
        ImU32 color;
        if (idx % 2 == 0) {
            color = IM_COL32(140, 80, 220, (int)(p.alpha * 255)); // purple
        } else {
            color = IM_COL32(50, 180, 160, (int)(p.alpha * 255)); // teal
        }

        draw->AddCircleFilled(
            ImVec2(winPos.x + p.x, winPos.y + p.y),
            p.radius,
            color
        );
    }
}

void GUI::sendMessage(const std::string& input) {
    isWaiting = true;
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
    // Format numbered lists in the complete response
    std::string formatted;
    for (size_t i = 0; i < fullReply.size(); i++) {
        if (i > 0 && isdigit((unsigned char)fullReply[i])) {
            size_t j = i;
            while (j < fullReply.size() && isdigit((unsigned char)fullReply[j])) j++;
           if (j < fullReply.size() && fullReply[j] == '.' && fullReply[i - 1] != '\n') {
                // Only add newline, not double newline
                if (i > 1 && fullReply[i - 2] != '\n') {
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
    isWaiting = false;
}
        );
    }).detach();
}

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

    // Font — 24px
    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 24.0f);

    // Style
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

        // Draw particles on background
        ImDrawList* bgDraw = ImGui::GetWindowDrawList();
        updateAndDrawParticles(bgDraw, ImGui::GetWindowPos(),
                               io.DisplaySize.x, io.DisplaySize.y);

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

void GUI::renderHeader() {
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, COL_HEADER);
    ImGui::BeginChild("##header", ImVec2(io.DisplaySize.x, 60.0f), false,
                      ImGuiWindowFlags_NoScrollbar);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();

    // Teal accent bar on left
    draw->AddRectFilled(ImVec2(p.x, p.y),
                        ImVec2(p.x + 4, p.y + 60),
                        IM_COL32(50, 200, 170, 255));

    // Title
    ImGui::SetCursorPos(ImVec2(20.0f, 8.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
    ImGui::Text("AI Chatbot");
    ImGui::PopStyleColor();

    // Subtitle
    ImGui::SetCursorPos(ImVec2(20.0f, 34.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.6f, 1.0f));
    ImGui::Text("Powered by Gemini 2.5");
    ImGui::PopStyleColor();

    // Status dot
    ImVec2 dotPos = ImVec2(p.x + io.DisplaySize.x - 24, p.y + 30);
    ImU32 dotColor = isWaiting ?
        IM_COL32(120, 120, 130, 255) :
        IM_COL32(50, 200, 120, 255);
    draw->AddCircleFilled(dotPos, 7.0f, dotColor);
    ImU32 glowColor = isWaiting ?
        IM_COL32(120, 120, 130, 60) :
        IM_COL32(50, 200, 120, 60);
    draw->AddCircleFilled(dotPos, 12.0f, glowColor);

    // Dropdown button — inside header
    ImGui::SetCursorPos(ImVec2(io.DisplaySize.x - 255.0f, 13.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,
        ApiClient::useOllama ?
        ImVec4(0.10f, 0.50f, 0.30f, 1.0f) :
        ImVec4(0.25f, 0.15f, 0.45f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        ApiClient::useOllama ?
        ImVec4(0.15f, 0.60f, 0.35f, 1.0f) :
        ImVec4(0.35f, 0.20f, 0.55f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
        ApiClient::useOllama ?
        ImVec4(0.08f, 0.40f, 0.25f, 1.0f) :
        ImVec4(0.20f, 0.10f, 0.35f, 1.0f));

    std::string buttonLabel = ApiClient::useOllama ?
        "Ollama: " + ApiClient::ollamaModel :
        "Gemini 2.5";
    // Pad label so arrow appears at right end
    while (buttonLabel.size() < 18) buttonLabel += " ";
    buttonLabel += "▼";

    if (ImGui::Button(buttonLabel.c_str(), ImVec2(200.0f, 40.0f))) {
        openModelPopup = true;  // set flag instead of OpenPopup;
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    // ← EndChild BEFORE popup
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

    // Popup OUTSIDE child window
    ImGui::SetNextWindowPos(ImVec2(
        winPos.x + io.DisplaySize.x - 200.0f,
        winPos.y + 61.0f
    ));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.12f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.40f, 0.20f, 0.60f, 1.0f));

    if (ImGui::BeginPopup("ModelSelectPopup")) {
        // Gemini section
        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
        ImGui::Text("Gemini");
        ImGui::PopStyleColor();
        ImGui::Separator();

        if (ImGui::Selectable("  Gemini 2.5 Flash", !ApiClient::useOllama)) {
            ApiClient::useOllama = false;
        }

        ImGui::Spacing();

        // Ollama section
        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
        ImGui::Text("Ollama (Local)");
        ImGui::PopStyleColor();
        ImGui::Separator();

        static const char* ollamaModels[] = {
            "llama3.2",
        };

        for (const char* model : ollamaModels) {
            bool selected = ApiClient::useOllama &&
                            ApiClient::ollamaModel == model;
            std::string label = std::string("  ") + model;
            if (ImGui::Selectable(label.c_str(), selected)) {
                ApiClient::useOllama = true;
                ApiClient::ollamaModel = model;
            }
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
}

void GUI::renderChatHistory() {
    ImGuiIO& io = ImGui::GetIO();
    float headerHeight = 61.0f;
    float inputHeight  = 70.0f;

    ImGui::SetCursorPos(ImVec2(0, headerHeight));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("ChatHistory",
                      ImVec2(io.DisplaySize.x, io.DisplaySize.y - headerHeight - inputHeight),
                      false);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 winPos    = ImGui::GetWindowPos();
    float winWidth   = io.DisplaySize.x;
    float padding    = 12.0f;
    float bubblePad  = 12.0f;
    float maxWidth   = winWidth * 0.65f;
    float scrollY    = ImGui::GetScrollY();

    ImGui::Dummy(ImVec2(0, 10.0f));

    for (const auto& msg : messages) {
        bool isUser = msg.size() >= 4 && msg.substr(0, 4) == "You:";
        std::string text = msg.size() > 5 ? msg.substr(5) : " ";

        // Calculate ideal width based on text
        ImVec2 singleLineSize = ImGui::CalcTextSize(text.c_str(), nullptr, false, -1.0f);
        float bubbleW;

        if (singleLineSize.x + bubblePad * 2 <= maxWidth) {
            // Text fits on one line — shrink bubble to fit
            bubbleW = singleLineSize.x + bubblePad * 2;
        } else {
            // Text needs wrapping — use maxWidth
            bubbleW = maxWidth;
        }

        float wrapW     = bubbleW - bubblePad * 2;
        ImVec2 textSize = ImGui::CalcTextSize(text.c_str(), nullptr, false, wrapW);
        float bubbleH   = textSize.y + bubblePad * 2;

        float cursorY   = ImGui::GetCursorPosY();
        float bubbleX   = isUser ? (winWidth - bubbleW - padding) : padding;

        // Screen coordinates
        ImVec2 bMin = ImVec2(winPos.x + bubbleX,          winPos.y + cursorY - scrollY);
        ImVec2 bMax = ImVec2(winPos.x + bubbleX + bubbleW, winPos.y + cursorY - scrollY + bubbleH);

        // Shadow
        draw->AddRectFilled(
            ImVec2(bMin.x + 3, bMin.y + 3),
            ImVec2(bMax.x + 3, bMax.y + 3),
            IM_COL32(0, 0, 0, 60), 10.0f);

        // Bubble
        ImU32 bubbleColor = isUser ? IM_COL32(64, 38, 115, 230) : IM_COL32(25, 64, 77, 230);
        draw->AddRectFilled(bMin, bMax, bubbleColor, 10.0f);

        // Border
        ImU32 borderColor = isUser ? IM_COL32(120, 60, 200, 180) : IM_COL32(50, 180, 160, 180);
        draw->AddRect(bMin, bMax, borderColor, 10.0f, 0, 1.5f);

        // Draw text directly using draw list — bypass ImGui layout entirely
        ImVec4 textColor = isUser ?
            ImVec4(COL_USER_TEXT.x, COL_USER_TEXT.y, COL_USER_TEXT.z, 1.0f) :
            ImVec4(COL_BOT_TEXT.x,  COL_BOT_TEXT.y,  COL_BOT_TEXT.z,  1.0f);

        ImVec2 textPos = ImVec2(bMin.x + bubblePad, bMin.y + bubblePad);
        draw->AddText(
            ImGui::GetFont(),
            ImGui::GetFontSize(),
            textPos,
            ImGui::ColorConvertFloat4ToU32(textColor),
            text.c_str(),
            nullptr,
            wrapW  // wrap width in pixels — this is the key
        );

        // Advance ImGui cursor
        ImGui::Dummy(ImVec2(winWidth, bubbleH + 8.0f));
    }

    // Animated waiting indicator
    if (isWaiting) {
        float time = (float)ImGui::GetTime();

        ImGui::Dummy(ImVec2(0, 5.0f));
        ImGui::SetCursorPosX(padding + bubblePad);

        // Draw three bouncing dots
        ImDrawList* dotDraw = ImGui::GetWindowDrawList();
        ImVec2 dotStart = ImGui::GetCursorScreenPos();
        float dotRadius = 5.0f;
        float dotSpacing = 18.0f;
        float bounceHeight = 6.0f;

        for (int d = 0; d < 3; d++) {
            // Each dot bounces with offset phase
            float phase = time * 4.0f - d * 0.4f;
            float bounce = sinf(phase) * bounceHeight;
            if (bounce < 0) bounce = 0; // only bounce up

            ImVec2 center = ImVec2(
                dotStart.x + d * dotSpacing + dotRadius,
                dotStart.y + dotRadius - bounce
            );

            // Alternate purple and teal
            ImU32 dotColor = (d % 2 == 0) ?
                IM_COL32(140, 80, 220, 220) :
                IM_COL32(50, 180, 160, 220);

            dotDraw->AddCircleFilled(center, dotRadius, dotColor);
        }

        // Advance cursor past the dots
        ImGui::Dummy(ImVec2(winWidth, dotRadius * 2 + bounceHeight + 8.0f));
    }
    // Auto scroll
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();       // ← closes "ChatHistory"
    ImGui::PopStyleColor();  // ← closes ChildBg color
}

void GUI::renderInputBox() {
    ImGuiIO& io = ImGui::GetIO();
    float inputAreaHeight = 70.0f;
    float inputY = io.DisplaySize.y - inputAreaHeight;

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();

    // Input area background
    draw->AddRectFilled(
        ImVec2(winPos.x, winPos.y + inputY - 1),
        ImVec2(winPos.x + io.DisplaySize.x, winPos.y + io.DisplaySize.y),
        IM_COL32(15, 15, 20, 255)
    );

    // Separator line
    draw->AddLine(
        ImVec2(winPos.x, winPos.y + inputY - 1),
        ImVec2(winPos.x + io.DisplaySize.x, winPos.y + inputY - 1),
        IM_COL32(80, 40, 120, 255), 1.5f
    );

    ImGui::SetCursorPos(ImVec2(15.0f, inputY + 14.0f));

    // Refocus input box if flagged
    if (refocusInput) {
        ImGui::SetKeyboardFocusHere(0);
        refocusInput = false;
    }

    // Input box
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
    ImGui::SetNextItemWidth(io.DisplaySize.x - 110.0f);
    bool entered = ImGui::InputText("##input", inputBuf, sizeof(inputBuf),
                                    ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopStyleVar();

    ImGui::SameLine(0, 10.0f);

    // Send button
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