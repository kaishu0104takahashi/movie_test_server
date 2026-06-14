cmake_minimum_required(VERSION 3.20)

project(movie_test_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# === パッケージの検索 ===
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavformat libavutil libswscale)
find_package(SDL2 REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(Threads REQUIRED)

# === ソースファイルの登録 ===
set(SOURCE_FILES
    src/main.cpp
    src/stream_app.cpp
    src/stream/udp_receiver.cpp
    src/stream/h264_decoder.cpp
    src/display/sdl_renderer.cpp
    src/stream/receiver_thread.cpp
)

# === 実行ファイルの作成 ===
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    PkgConfig::FFMPEG
    SDL2::SDL2
    spdlog::spdlog
    fmt::fmt
    Threads::Threads
)