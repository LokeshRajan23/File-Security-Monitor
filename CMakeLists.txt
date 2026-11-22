cmake_minimum_required(VERSION 3.16)

project(FileMonitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(CURL REQUIRED)

set(COMMON_SOURCES
    main.cpp
    Factory.cpp
    UserInput.cpp
    UserInput.h
    FileMonitor.h
    EmailNotifier.cpp
    EmailNotifier.h
)

if(WIN32)
    list(APPEND COMMON_SOURCES
        FileMonitorWindows.cpp
        FileMonitorWindows.h
    )
elseif(UNIX)
    list(APPEND COMMON_SOURCES
        FileMonitorLinux.cpp
        FileMonitorLinux.h
    )
else()
    message(FATAL_ERROR "Unsupported operating system")
endif()

add_executable(FileMonitor ${COMMON_SOURCES})

target_link_libraries(FileMonitor
    PRIVATE
    Threads::Threads
    CURL::libcurl
)