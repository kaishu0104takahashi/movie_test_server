cmake_minimum_required(VERSION 3.10)
project(movie_test_server)

# C++のバージョン指定
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 必要なパッケージ（FFmpeg, SDL2, SDL2_ttf）の検索
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavutil)

# SDL2とSDL2_ttfの検索
find_package(SDL2 REQUIRED)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)

# インクルードディレクトリの設定
include_directories(
    include
    ${FFMPEG_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
)

# srcフォルダ内のすべての .cpp ファイルを再帰的に取得
file(GLOB_RECURSE SOURCES "src/*.cpp")

# 実行ファイルの生成
add_executable(movie_test_server ${SOURCES})

# ライブラリのリンク（SDL2_ttf を追加）
target_link_libraries(movie_test_server
    ${FFMPEG_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    pthread
)