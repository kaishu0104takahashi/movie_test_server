#ifndef SDL_RENDERER_HPP_
#define SDL_RENDERER_HPP_

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
extern "C" {
#include <libavutil/frame.h>
}

#include "stream/control_relay.hpp"

class SdlRenderer {
public:
    SdlRenderer(const std::string& title, int width, int height);
    ~SdlRenderer();

    void render_frame(AVFrame* frame, const ControlState& state);
    bool poll_events();

private:
    int width_;
    int height_;
    SDL_Window* window_;
    SDL_Renderer* renderer_;
    SDL_Texture* texture_;

    TTF_Font* font_ = nullptr;
    bool show_overlay_ = true;
    int current_frame_width_ = 0;
    int current_frame_height_ = 0;

    void draw_text(const std::string& text, int x, int y, SDL_Color color);
    void fill_circle(int cx, int cy, int radius, SDL_Color color); // ★追加: 円の描画
};

#endif