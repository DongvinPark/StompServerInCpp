cmake_minimum_required(VERSION 3.16)

# Project name and version
project(StompServerInCpp VERSION 1.0)

# Set C++ standard to 17 : boost::redis 를 써야 하기 때문에 C++17 이상으로 설정해야 한다.
set(CMAKE_CXX_STANDARD 17)
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

    # Find Boost and OpenSSL(for redis)
    find_package(Boost REQUIRED COMPONENTS system)
    find_package(OpenSSL REQUIRED)

    # Additional libraries required on Windows
    set(PLATFORM_LIBS
            ws2_32
            OpenSSL::SSL
            OpenSSL::Crypto
    )

elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin") # M chip Mac
    # Find Boost using Homebrew (macOS only)
    find_package(Boost REQUIRED)
    find_package(OpenSSL REQUIRED) # for boost::redis

    set(PLATFORM_LIBS
            OpenSSL::SSL
            OpenSSL::Crypto
    )

elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Find Boost
    find_package(Boost REQUIRED)

    # Link pthread library for Linux
    set(PLATFORM_LIBS pthread)
    find_package(OpenSSL REQUIRED)

    set(PLATFORM_LIBS
            pthread
            OpenSSL::SSL
            OpenSSL::Crypto
    )

endif()

# Include directories
include_directories(${Boost_INCLUDE_DIRS} src)

# Add the executable
# 소스 파일들은 각각 int main(){...} 을 가지고 있기 때문에
# add_executable 안에는 오직 1 개의 소스 파일만 주석 해제 돼 있어야 한다.
add_executable(StompServerInCpp
        src/main.cpp
        constants/C.h
        include/Logger.h
        src/util/Logger.cpp
        constants/Util.h
        include/PeriodicTask.h
        src/timer/PeriodicTask.cpp
        src/redis/RedisService.h
        src/service/StompHandler.h
        include/Buffer.h
        src/service/Session.h
        src/server/Server.h
        src/util/Buffer.cpp
)

# Link Boost libraries and platform-specific libraries
target_link_libraries(StompServerInCpp ${Boost_LIBRARIES} ${PLATFORM_LIBS})