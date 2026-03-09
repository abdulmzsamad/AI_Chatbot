cmake_minimum_required(VERSION 3.15)
project(AIChatbot)

set(CMAKE_CXX_STANDARD 17)

# Find packages
find_package(CURL REQUIRED)

# Include directories
include_directories(include)
include_directories(lib)
include_directories(lib/backends)

# ImGui source files
set(IMGUI_SOURCES
    lib/imgui.cpp
    lib/imgui_draw.cpp
    lib/imgui_tables.cpp
    lib/imgui_widgets.cpp
    lib/backends/imgui_impl_opengl3.cpp
    lib/backends/imgui_impl_win32.cpp
)

# Your source files
file(GLOB SOURCES "src/*.cpp")

add_executable(chatbot WIN32 ${SOURCES} ${IMGUI_SOURCES})

target_link_libraries(chatbot
    CURL::libcurl
    opengl32
    dwmapi
)