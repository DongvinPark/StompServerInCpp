cmake_minimum_required(VERSION 3.16)

# Project name and version
project(StompServerInCpp VERSION 1.0)

# Set C++ standard to 14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add platform-specific settings
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # MSVC가 소스 파일을 UTF-8로 해석하도록 설정.
    # Windows 기본 코드 페이지(CP949)로 인한 한글 주석/문자열
    # encoding 문제 및 연쇄적인 syntax error를 방지한다.
    add_compile_options(/utf-8)

    # Set toolchain for vcpkg (Windows only)
    set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

    # Find Boost using vcpkg
    find_package(Boost REQUIRED COMPONENTS system)

    # Additional libraries required on Windows
    set(PLATFORM_LIBS ws2_32)

elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # Find Boost using Homebrew (macOS only)
    find_package(Boost REQUIRED)

    # No additional libraries required for macOS
    set(PLATFORM_LIBS "")

elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Find Boost
    find_package(Boost REQUIRED)

    # Link pthread library for Linux
    set(PLATFORM_LIBS pthread)
endif()

# Include directories
include_directories(${Boost_INCLUDE_DIRS} src)

# Add the executable
# 소스 파일들은 각각 int main(){...} 을 가지고 있기 때문에
# add_executable 안에는 오직 1 개의 소스 파일만 주석 해제 돼 있어야 한다.
add_executable(StompServerInCpp
        src/main.cpp
)

# Link Boost libraries and platform-specific libraries
target_link_libraries(StompServerInCpp ${Boost_LIBRARIES} ${PLATFORM_LIBS})