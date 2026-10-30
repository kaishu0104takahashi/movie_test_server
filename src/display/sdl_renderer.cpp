#include "display/sdl_renderer.hpp"
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <algorithm>

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
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = static_cast<int>(std::sqrt(radius * radius - dy * dy));
        SDL_RenderDrawLine(renderer_, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

void SdlRenderer::fill_rect(int x, int y, int w, int h, SDL_Color color) {
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(renderer_, &rect);
}

void SdlRenderer::draw_rect(int x, int y, int w, int h, SDL_Color color) {
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderDrawRect(renderer_, &rect);
}

void SdlRenderer::render_frame(AVFrame* frame, const ControlState& state) {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    // 映像の描画
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

    // 3分割 HUD（メーター）の描画
    if (show_overlay_ && font_) {
        int win_w, win_h;
        SDL_GetWindowSize(window_, &win_w, &win_h);

        // カラー定義
        SDL_Color bg_color = {0, 0, 0, 180}; // 半透明の黒
        SDL_Color white = {255, 255, 255, 255};
        SDL_Color green = {0, 255, 0, 255};
        SDL_Color red = {255, 0, 0, 255};
        SDL_Color gray = {150, 150, 150, 255};
        SDL_Color blue = {0, 150, 255, 255};

        // レイアウト定義 (画面下部に3つのボックスを配置)
        int margin = 20;
        int spacing = 20;
        int box_h = 190; // ボックスの高さ
        int box_w = (win_w - margin * 2 - spacing * 2) / 3;
        int box_y = win_h - box_h - margin;

        char buf[128];

        // トップ左のヘルプテキスト
        draw_text("[TAB] Toggle HUD | [ESC] Fullscreen | [Q] Quit", 10, 10, white);

        // ==============================================================
        // ① 左ボックス：速度メーター ＆ 操作インジケータ
        // ==============================================================
        int left_x = margin;
        fill_rect(left_x, box_y, box_w, box_h, bg_color);
        draw_rect(left_x, box_y, box_w, box_h, white);
        
        draw_text("Speed & Control", left_x + 10, box_y + 10, white);

        // 速度とクルーズ状態
        snprintf(buf, sizeof(buf), "Speed: %d km /h  ", state.target_speed);
        draw_text(buf, left_x + 10, box_y + 45, green);
        snprintf(buf, sizeof(buf), "  Cruise: %s", state.cruise_set ? "ON" : "OFF");
        draw_text(buf, left_x + 200, box_y + 45, state.cruise_set ? green : gray);

        // メーターバーの座標計算
        int bar_label_w = 60;
        int bar_x = left_x + bar_label_w + 10;
        int bar_w = box_w - bar_label_w - 30;
        int bar_h = 14;

        // アクセル (1.0 = 0% / -1.0 = 100%)
        float throt_ratio = std::max(0.0f, std::min(1.0f, (1.0f - state.throttle) / 2.0f));
        draw_text("THR", left_x + 10, box_y + 85, white);
        draw_rect(bar_x, box_y + 90, bar_w, bar_h, gray);
        fill_rect(bar_x, box_y + 90, static_cast<int>(bar_w * throt_ratio), bar_h, green);

        // ブレーキ (1.0 = 0% / -1.0 = 100%)
        float brake_ratio = std::max(0.0f, std::min(1.0f, (1.0f - state.brake) / 2.0f));
        draw_text("BRK", left_x + 10, box_y + 115, white);
        draw_rect(bar_x, box_y + 120, bar_w, bar_h, gray);
        fill_rect(bar_x, box_y + 120, static_cast<int>(bar_w * brake_ratio), bar_h, red);

        // ステアリング (-1.0 = 左 / 0.0 = 中央 / 1.0 = 右)
        float steer_val = std::max(-1.0f, std::min(1.0f, state.steer));
        int steer_px = static_cast<int>(steer_val * (bar_w / 2.0f));
        draw_text("STR", left_x + 10, box_y + 145, white);
        draw_rect(bar_x, box_y + 150, bar_w, bar_h, gray);
        fill_rect(bar_x + bar_w / 2, box_y + 145, 2, bar_h + 10, white); // センターライン
        if (steer_px < 0) {
            fill_rect(bar_x + bar_w / 2 + steer_px, box_y + 150, -steer_px, bar_h, blue);
        } else {
            fill_rect(bar_x + bar_w / 2, box_y + 150, steer_px, bar_h, blue);
        }

        // ==============================================================
        // ② 中央ボックス：電力量メーター（予約領域）
        // ==============================================================
        int center_x = left_x + box_w + spacing;
        fill_rect(center_x, box_y, box_w, box_h, bg_color);
        draw_rect(center_x, box_y, box_w, box_h, white);
        draw_text("Power (Reserved)", center_x + 10, box_y + 10, gray);
        
        // ※将来的にここに電圧やバッテリー残量のゲージを描画できます
        draw_text("No Data", center_x + box_w / 2 - 45, box_y + box_h / 2 - 10, gray);

        // ==============================================================
        // ③ 右ボックス：通信状態 ＆ センサーアラート
        // ==============================================================
        int right_x = center_x + box_w + spacing;
        fill_rect(right_x, box_y, box_w, box_h, bg_color);
        draw_rect(right_x, box_y, box_w, box_h, white);
        draw_text("System Status", right_x + 10, box_y + 10, white);

        snprintf(buf, sizeof(buf), "Camera  : %s", state.cam_on ? "ON" : "OFF");
        draw_text(buf, right_x + 10, box_y + 50, state.cam_on ? green : red);

        snprintf(buf, sizeof(buf), "Cockpit : %s", state.cockpit_connected ? "Connected" : "Disconnected");
        draw_text(buf, right_x + 10, box_y + 90, state.cockpit_connected ? green : red);

        snprintf(buf, sizeof(buf), "Vehicle : %s", state.client_connected ? "Connected" : "Disconnected");
        draw_text(buf, right_x + 10, box_y + 130, state.client_connected ? green : red);

        // 障害物検知アラート (右下に配置)
        if (state.distance_alert == 1) {
            draw_text("ALERT", right_x + box_w - 90, box_y + 15, red);
            fill_circle(right_x + box_w - 30, box_y + 28, 12, red);
        }
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
                Uint32 is_fullscreen = SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                SDL_SetWindowFullscreen(window_, is_fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
            }
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