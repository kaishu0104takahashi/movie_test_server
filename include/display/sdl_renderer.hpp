/**
 * @file sdl_renderer.hpp
 * @brief 解凍された生ピクセル（YUV）を画面に超低遅延で描画する部品
 */

 #ifndef SDL_RENDERER_HPP_
 #define SDL_RENDERER_HPP_
 
 #include <SDL2/SDL.h>
 #include <string>
 
 // 解凍機から出てくるデータの形（YuvFrame）を知るために読み込む
 #include "stream/h264_decoder.hpp"
 
 class SdlRenderer {
 public:
     // ① 準備：ウィンドウの名前、幅、高さを指定して画面を作る
     SdlRenderer(const std::string& title, int width, int height);
     
     // ② 後片付け：ウィンドウを綺麗に閉じる
     ~SdlRenderer();
 
     // コピー禁止の安全装置
     SdlRenderer(const SdlRenderer&) = delete;
     SdlRenderer& operator=(const SdlRenderer&) = delete;
 
     // ③ メイン処理：解凍されたYUVデータを受け取って、画面に表示する
     void render_yuv(const H264Decoder::YuvFrame& frame);
 
     // ④ OSの仕事：ウィンドウの「×」ボタンが押されたかなどを監視する
     bool poll_events();
 
 private:
     SDL_Window* window_ = nullptr;     // ウィンドウの枠
     SDL_Renderer* renderer_ = nullptr; // 描画する筆（GPU）
     SDL_Texture* texture_ = nullptr;   // 映像を貼り付けるキャンバス
 
     int width_;
     int height_;
 };
 
 #endif