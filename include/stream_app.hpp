/**
 * @file stream_app.hpp
 * @brief 受信と描画を統合して管理するアプリケーションクラス
 */
#ifndef STREAM_APP_HPP_
#define STREAM_APP_HPP_

#include <atomic>
#include "stream/receiver_thread.hpp"
#include "display/sdl_renderer.hpp"

class StreamApp {
public:
    StreamApp(int port, const std::string& title, int width, int height, DecodeMode mode);
    ~StreamApp() = default;

    void run(const std::atomic<bool>& keep_running);

private:
    ReceiverThread receiver_;
    SdlRenderer renderer_;
};

#endif