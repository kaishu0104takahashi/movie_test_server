/**
 * @file stream_app.hpp
 * @brief 受信と描画を統合して管理するアプリケーションクラス
 */

 #ifndef STREAM_APP_HPP_
 #define STREAM_APP_HPP_
 
 #include <atomic>
 #include <vector>
 #include <cstdint>
 #include "stream/receiver_thread.hpp"
 #include "display/sdl_renderer.hpp"
 
 class StreamApp {
 public:
     StreamApp(int port, const std::string& title, int width, int height);
     ~StreamApp() = default;
 
     // アプリケーションのメインループを実行する
     // main.cppの安全装置（keep_running）を監視しながら動く
     void run(const std::atomic<bool>& keep_running);
 
 private:
     ReceiverThread receiver_;
     SdlRenderer renderer_;
 
     // 描画用の一時バッファ（main.cppからここに隠蔽！）
     std::vector<uint8_t> y_buf_, u_buf_, v_buf_;
     int y_pitch_ = 0, u_pitch_ = 0, v_pitch_ = 0;
     int width_ = 0, height_ = 0;
     H264Decoder::YuvFrame frame_;
 };
 
 #endif