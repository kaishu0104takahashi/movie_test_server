#include "display/sdl_renderer.hpp"
#include <stdexcept>
#include <iostream>

SdlRenderer::SdlRenderer(const std::string& title, int width, int height)
    : width_(width), height_(height), texture_(nullptr) {
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        throw std::runtime_error(std::string("エラー: SDL2の初期化に失敗 -> ") + SDL_GetError());
    }

    if (TTF_Init() == -1) {
        throw std::runtime_error(std::string("エラー: SDL2_ttfの初期化に失敗 -> ") + TTF_GetError());
    }

    // ラズパイ標準のフォントを読み込み（サイズ24）
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

void SdlRenderer::render_frame(AVFrame* frame, const ControlState& state) {
    // 映像サイズが変わった（または初回）場合のテクスチャ再生成
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

    // YUV映像の更新と描画
    SDL_UpdateYUVTexture(
        texture_, nullptr,
        frame->data[0], frame->linesize[0],
        frame->data[1], frame->linesize[1],
        frame->data[2], frame->linesize[2]
    );

    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);

    // テキストのオーバーレイ描画
    if (show_overlay_ && font_) {
        SDL_Color green = {0, 255, 0, 255};
        SDL_Color red = {255, 0, 0, 255};
        SDL_Color white = {255, 255, 255, 255};
        
        char buf[128];
        snprintf(buf, sizeof(buf), "STR: %.2f | THR: %.2f | BRK: %.2f | HRN: %d", 
                 state.steer, state.throttle, state.brake, state.horn);
        draw_text(buf, 20, 20, green);
        
        snprintf(buf, sizeof(buf), "CAM: %s", state.cam_on ? "ON" : "OFF");
        draw_text(buf, 20, 50, state.cam_on ? green : red);
        
        draw_text("[TAB] Toggle Overlay", 20, 80, white);
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
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                return false;
            }
            if (event.key.keysym.sym == SDLK_TAB) {
                show_overlay_ = !show_overlay_; // TABキーでON/OFF切り替え
            }
        }
    }
    return true;
}