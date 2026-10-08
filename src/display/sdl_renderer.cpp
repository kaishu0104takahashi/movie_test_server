#include "display/sdl_renderer.hpp"
#include <stdexcept>
#include <iostream>
#include <cmath> // 円の描画計算用

SdlRenderer::SdlRenderer(const std::string& title, int width, int height)
    : width_(width), height_(height), texture_(nullptr) {
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        throw std::runtime_error(std::string("エラー: SDL2の初期化に失敗 -> ") + SDL_GetError());
    }

    if (TTF_Init() == -1) {
        throw std::runtime_error(std::string("エラー: SDL2_ttfの初期化に失敗 -> ") + TTF_GetError());
    }

    font_ = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 24);
    if (!font_) {
        std::cerr << "Warning: Failed to load font. Overlay will not be shown." << std::endl;
    }

    window_ = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width_, height_,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window_) {
        throw std::runtime_error(std::string("エラー: ウィンドウの作成に失敗 -> ") + SDL_GetError());
    }

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        throw std::runtime_error(std::string("エラー: レンダラーの作成に失敗 -> ") + SDL_GetError());
    }
}

SdlRenderer::~SdlRenderer() {
    if (font_) TTF_CloseFont(font_);
    TTF_Quit();
    if (texture_) SDL_DestroyTexture(texture_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

void SdlRenderer::draw_text(const std::string& text, int x, int y, SDL_Color color) {
    if (!font_) return;
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font_, text.c_str(), color);
    if (!surface) return;
    
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_Rect rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer_, tex, nullptr, &rect);
    
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surface);
}

void SdlRenderer::fill_circle(int cx, int cy, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = static_cast<int>(std::sqrt(radius * radius - dy * dy));
        SDL_RenderDrawLine(renderer_, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

void SdlRenderer::render_frame(AVFrame* frame, const ControlState& state) {
    // 描画領域をクリア（背景を黒にする）
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    if (state.cam_on == 1 && frame != nullptr) {
        if (!texture_ || current_frame_width_ != frame->width || current_frame_height_ != frame->height) {
            if (texture_) SDL_DestroyTexture(texture_);
            current_frame_width_ = frame->width;
            current_frame_height_ = frame->height;
            texture_ = SDL_CreateTexture(
                renderer_,
                SDL_PIXELFORMAT_IYUV,
                SDL_TEXTUREACCESS_STREAMING,
                current_frame_width_,
                current_frame_height_
            );
        }

        SDL_UpdateYUVTexture(
            texture_, nullptr,
            frame->data[0], frame->linesize[0],
            frame->data[1], frame->linesize[1],
            frame->data[2], frame->linesize[2]
        );

        SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    }
    else if (state.cam_on == 0) {
        if (font_) {
            SDL_Color yellow = {255, 255, 0, 255};
            std::string stop_msg = "Camera Stopped";
            
            int text_w = 0, text_h = 0;
            TTF_SizeUTF8(font_, stop_msg.c_str(), &text_w, &text_h);
            
            int win_w, win_h;
            SDL_GetWindowSize(window_, &win_w, &win_h);
            
            draw_text(stop_msg, (win_w - text_w) / 2, (win_h - text_h) / 2, yellow);
        }
    }

    if (show_overlay_ && font_) {
        SDL_Color green = {0, 255, 0, 255};
        SDL_Color red = {255, 0, 0, 255};
        SDL_Color white = {255, 255, 255, 255};
        
        char buf[128];
        snprintf(buf, sizeof(buf), "STR: %.2f | THR: %.2f | BRK: %.2f | HRN: %d", 
                 state.steer, state.throttle, state.brake, state.horn);
        draw_text(buf, 20, 20, green);
        
        snprintf(buf, sizeof(buf), "CRUISE - SET:%d OFF:%d UP:%d DOWN:%d", 
                 state.cruise_set, state.cruise_off, state.cruise_speed_up, state.cruise_speed_down);
        draw_text(buf, 20, 50, green);
        
        snprintf(buf, sizeof(buf), "CAM: %s", state.cam_on ? "ON" : "OFF");
        draw_text(buf, 20, 80, state.cam_on ? green : red);
        
        draw_text("[TAB] Toggle Overlay | [ESC] Toggle Fullscreen | [Q] Quit", 20, 110, white);

        int win_w, win_h;
        SDL_GetWindowSize(window_, &win_w, &win_h);

        char cockpit_buf[64];
        snprintf(cockpit_buf, sizeof(cockpit_buf), "cockpit:%s", state.cockpit_connected ? "connected" : "disconnected");
        int cw = 0, ch = 0;
        TTF_SizeUTF8(font_, cockpit_buf, &cw, &ch);
        draw_text(cockpit_buf, win_w - cw - 20, 20, state.cockpit_connected ? green : red);

        char client_buf[64];
        snprintf(client_buf, sizeof(client_buf), "rpi5-client:%s", state.client_connected ? "connected" : "disconnected");
        int rw = 0, rh = 0;
        TTF_SizeUTF8(font_, client_buf, &rw, &rh);
        draw_text(client_buf, win_w - rw - 20, 20 + ch + 10, state.client_connected ? green : red);
    }

    if (state.distance_alert == 1) {
        int win_w, win_h;
        SDL_GetWindowSize(window_, &win_w, &win_h);
        
        SDL_Color red = {255, 0, 0, 255};
        fill_circle(win_w - 50, 120, 30, red);
    }

    SDL_RenderPresent(renderer_);
}

bool SdlRenderer::poll_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return false;
        }
        if (event.type == SDL_KEYDOWN) {
            // ★変更: ESCキーでフルスクリーン切替
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                Uint32 is_fullscreen = SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                SDL_SetWindowFullscreen(window_, is_fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
            }
            // ★追加: Qキーでアプリ終了
            if (event.key.keysym.sym == SDLK_q) {
                return false;
            }
            if (event.key.keysym.sym == SDLK_TAB) {
                show_overlay_ = !show_overlay_; 
            }
        }
    }
    return true;
}