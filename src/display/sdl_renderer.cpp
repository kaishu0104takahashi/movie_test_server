#include "display/sdl_renderer.hpp"
#include <stdexcept>
#include <iostream>

SdlRenderer::SdlRenderer(const std::string& title, int width, int height)
    : width_(width), height_(height) {
    
    // 1. SDL2（描画システム）を初期化する
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        throw std::runtime_error(std::string("エラー: SDL2の初期化に失敗 -> ") + SDL_GetError());
    }

    // 2. ウィンドウ（画面の枠）を作成する
    window_ = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, // 画面の中央に配置
        width_, height_,
        SDL_WINDOW_SHOWN
    );
    if (!window_) {
        throw std::runtime_error(std::string("エラー: ウィンドウの作成に失敗 -> ") + SDL_GetError());
    }

    // 3. 描画する筆（GPUアクセラレーションを有効化）を作成する
    // ※ SDL_RENDERER_ACCELERATED により、ラズパイのGPUを使って超高速に描画します
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        throw std::runtime_error(std::string("エラー: レンダラーの作成に失敗 -> ") + SDL_GetError());
    }

    // 4. 映像を貼り付けるキャンバス（テクスチャ）を作成する
    // ★ ここが超重要: H.264から出てきた「YUV420P」という形式を、そのままGPUに流し込める専用キャンバスです
    texture_ = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_IYUV, // YUV形式専用
        SDL_TEXTUREACCESS_STREAMING, // 毎フレーム映像が書き換わる「動画用」設定
        width_, height_
    );
    if (!texture_) {
        throw std::runtime_error(std::string("エラー: テクスチャの作成に失敗 -> ") + SDL_GetError());
    }
}

SdlRenderer::~SdlRenderer() {
    // 綺麗にお掃除
    if (texture_) SDL_DestroyTexture(texture_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

void SdlRenderer::render_yuv(const H264Decoder::YuvFrame& frame) {
    // 1. GPUのキャンバス（テクスチャ）に、解凍された生のYUVデータを直接流し込む！
    SDL_UpdateYUVTexture(
        texture_,
        nullptr,
        frame.y_data, frame.y_pitch,
        frame.u_data, frame.u_pitch,
        frame.v_data, frame.v_pitch
    );

    // 2. 画面を一度真っ黒にクリアする
    SDL_RenderClear(renderer_);

    // 3. 映像が描き込まれたキャンバスを、ウィンドウ全体に貼り付ける
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);

    // 4. 画面に表示！（ここでパッと映像が出ます）
    SDL_RenderPresent(renderer_);
}

bool SdlRenderer::poll_events() {
    SDL_Event e;
    // OSから「ウィンドウの×ボタンが押された」「キーボードが押された」などのイベントを読み取る
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            return false; // 「終了してくれ」という合図
        }
        // もし「ESCキー」が押されたら終了する（おまけ機能）
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
            return false;
        }
    }
    return true; // まだ続けてOK
}