cmake_minimum_required(VERSION 3.16)
project(sync C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Общие, кроссплатформенные исходники.
set(SOURCES
    src/main.c
    src/relay.c
    src/client.c
    src/net.c
)

# Платформенный слой ввода: компилируем только нужный файл.
if(APPLE)
    list(APPEND SOURCES src/input_macos.c)
elseif(WIN32)
    list(APPEND SOURCES src/input_windows.c)
else()
    list(APPEND SOURCES src/input_stub.c)
endif()

add_executable(sync ${SOURCES})

if(APPLE)
    # CoreGraphics event tap и run loop.
    target_link_libraries(sync "-framework ApplicationServices")
elseif(WIN32)
    # Winsock.
    target_link_libraries(sync ws2_32)
else()
    # Linux: только POSIX-сокеты и потоки, ничего лишнего.
    find_package(Threads REQUIRED)
    target_link_libraries(sync Threads::Threads)
endif()

# pthread нужен и на macOS для сетевого потока клиента
# (на macOS обычно линкуется неявно, но добавим явно для надёжности).
if(APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(sync Threads::Threads)
endif()
