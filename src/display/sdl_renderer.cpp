#include "display/sdl_renderer.hpp"
#include <stdexcept>
#include <iostream>

SdlRenderer::SdlRenderer(const std::string& title, int width, int height)
    : width_(width), height_(height), texture_(nullptr) { // ← texture_を初期化
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        throw std::runtime_error(std::string("エラー: SDL2の初期化に失敗 -> ") + SDL_GetError());
    }

    window_ = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width_, height_, // ウィンドウサイズはそのまま（大きく表示するため）
        SDL_WINDOW_SHOWN
    );
    if (!window_) {
        throw std::runtime_error(std::string("エラー: ウィンドウの作成に失敗 -> ") + SDL_GetError());
    }

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        throw std::runtime_error(std::string("エラー: レンダラーの作成に失敗 -> ") + SDL_GetError());
    }

    // ★修正箇所: ここにあった SDL_CreateTexture を削除しました！
    // 理由は「実際の映像のサイズが来るまで、テクスチャの正しいサイズが分からないから」です。
}

SdlRenderer::~SdlRenderer() {
    if (texture_) SDL_DestroyTexture(texture_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

void SdlRenderer::render_frame(const AVFrame* frame) {
    if (!frame) return;

    // ★修正箇所: テクスチャが無い、または映像のサイズが変わった場合に動的に生成する
    int tex_w = 0, tex_h = 0;
    if (texture_) {
        SDL_QueryTexture(texture_, nullptr, nullptr, &tex_w, &tex_h);
    }

    if (!texture_ || tex_w != frame->width || tex_h != frame->height) {
        if (texture_) {
            SDL_DestroyTexture(texture_);
        }
        // 届いた映像のサイズ (frame->width) と完全に一致するテクスチャを作る
        texture_ = SDL_CreateTexture(
            renderer_,
            SDL_PIXELFORMAT_IYUV,
            SDL_TEXTUREACCESS_STREAMING,
            frame->width, frame->height
        );
        if (!texture_) {
            std::cerr << "テクスチャ再生成エラー: " << SDL_GetError() << std::endl;
            return;
        }
        std::cout << "[SdlRenderer] 映像サイズに合わせてテクスチャを生成しました: " 
                  << frame->width << "x" << frame->height << std::endl;
    }

    // YUVデータをテクスチャに流し込む
    SDL_UpdateYUVTexture(
        texture_,
        nullptr,
        frame->data[0], frame->linesize[0],
        frame->data[1], frame->linesize[1],
        frame->data[2], frame->linesize[2]
    );

    SDL_RenderClear(renderer_);
    
    // テクスチャ（160x120）をウィンドウ（640x480）いっぱいに自動拡大して描画
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    
    SDL_RenderPresent(renderer_);
}

bool SdlRenderer::poll_events() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            return false;
        }
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
            return false;
        }
    }
    return true;
}