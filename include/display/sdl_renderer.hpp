/**
 * @file sdl_renderer.hpp
 * @brief 解凍された生ピクセルを画面に超低遅延で描画する部品
 */
#ifndef SDL_RENDERER_HPP_
#define SDL_RENDERER_HPP_

#include <SDL2/SDL.h>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
}

class SdlRenderer {
public:
    SdlRenderer(const std::string& title, int width, int height);
    ~SdlRenderer();

    SdlRenderer(const SdlRenderer&) = delete;
    SdlRenderer& operator=(const SdlRenderer&) = delete;

    // AVFrame のポインタを直接受け取るように変更
    void render_frame(const AVFrame* frame);
    bool poll_events();

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    int width_;
    int height_;
};

#endif