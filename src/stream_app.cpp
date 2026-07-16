#include "stream_app.hpp"
#include <iostream>
#include <chrono>

StreamApp::StreamApp(int port, const std::string& title, int width, int height, DecodeMode mode) {
    receiver_ = std::make_unique<ReceiverThread>(port, mode);
    renderer_ = std::make_unique<SdlRenderer>(title, width, height);

    // 車両のグローバルIP（適宜環境に合わせて変更してください）
    std::string car_global_ip = "192.168.77.233"; 
    
    // 5005番(操作)と5678番(カメラ)で受信し、車両のIPへリレーする
    relay_ = std::make_unique<ControlRelay>(5005, 5678, car_global_ip, 5005, 5678);
}

StreamApp::~StreamApp() {
    // std::unique_ptr により自動的に破棄されます
}

void StreamApp::run(std::atomic<bool>& keep_running) {
    receiver_->start();

    while (keep_running) {
        if (!renderer_->poll_events()) {
            keep_running = false;
            break;
        }

        AVFrame* frame = nullptr;
        if (receiver_->get_latest_frame(&frame) && frame != nullptr) {
            
            // リレーモジュールから最新の操作データを取得し、描画に渡す
            ControlState state = relay_->get_current_state();
            renderer_->render_frame(frame, state);
            
            av_frame_free(&frame);
        }
        
        // 描画ループの負荷軽減 (約30fps)
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
    receiver_->stop();
}