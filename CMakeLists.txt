cmake_minimum_required(VERSION 3.15)
project(AIChatbot)

set(CMAKE_CXX_STANDARD 17)

# Include headers
include_directories(include)
include_directories(lib)

# Find curl
find_package(CURL REQUIRED)

# Source files
file(GLOB SOURCES "src/*.cpp")

add_executable(chatbot ${SOURCES})

target_link_libraries(chatbot CURL::libcurl)