#include "stream_app.hpp"
#include <iostream>
#include <chrono>

StreamApp::StreamApp(int port, const std::string& title, int width, int height, DecodeMode mode) {
    receiver_ = std::make_unique<ReceiverThread>(port, mode);
    renderer_ = std::make_unique<SdlRenderer>(title, width, height);

    // 車両のグローバルIP（適宜環境に合わせて変更してください）
    std::string car_global_ip = "219.112.66.121"; 
    
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

        // リレーモジュールから最新の操作データを取得
        ControlState state = relay_->get_current_state();
        
        // カメラ状態をReceiverThreadに伝達（OFFならデコード停止）
        receiver_->set_active(state.cam_on == 1);

        AVFrame* frame = nullptr;
        bool got_frame = receiver_->get_latest_frame(&frame);

        if (state.cam_on == 1 && got_frame && frame != nullptr) {
            // カメラON かつ フレームが存在する場合は映像を描画
            renderer_->render_frame(frame, state);
            av_frame_free(&frame);
        } else {
            // カメラOFF、またはフレーム未到達の場合は映像なしで描画を呼ぶ
            if (frame != nullptr) {
                av_frame_free(&frame);
            }
            renderer_->render_frame(nullptr, state);
        }
        
        // 描画ループの負荷軽減 (約30fps)
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
    receiver_->stop();
}