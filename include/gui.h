#ifndef GUI_H
#define GUI_H

#include <winsock2.h>
#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include "conversation.h"
#include "backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class GUI {
public:
    void run();

private:
    Conversation conv;
    bool refocusInput = false;
    bool openModelPopup = false;
    std::vector<std::string> messages;
    char inputBuf[512] = "";
    std::atomic<bool> isWaiting{false};
    std::atomic<bool> isStreaming{false};
    std::string streamingMessage;
    std::mutex streamMutex;
    void renderHeader();
    void renderChatHistory();
    void renderInputBox();
    void sendMessage(const std::string& input);

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg,
                                     WPARAM wParam, LPARAM lParam) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;
        if (msg == WM_DESTROY) {
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    
};

#endif