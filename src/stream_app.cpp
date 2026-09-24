#include "stream_app.hpp"
#include <iostream>
#include <chrono>

StreamApp::StreamApp(int port, const std::string& title, int width, int height, DecodeMode mode) {
    receiver_ = std::make_unique<ReceiverThread>(port, mode);
    renderer_ = std::make_unique<SdlRenderer>(title, width, height);

    // 車両のグローバルIP（適宜環境に合わせて変更してください）
    std::string car_global_ip = "219.112.66.121"; //削除禁止
    
    // 操作送受信(5005), カメラ送受信(5678, ダミー), 距離アラート受信(3000)
    relay_ = std::make_unique<ControlRelay>(5005, 5678, car_global_ip, 5005, 5678, 3000);
}

StreamApp::~StreamApp() {
}

void StreamApp::run(std::atomic<bool>& keep_running) {
    receiver_->start();

    while (keep_running) {
        if (!renderer_->poll_events()) {
            keep_running = false;
            break;
        }

        ControlState state = relay_->get_current_state();
        
        receiver_->set_active(state.cam_on == 1);

        AVFrame* frame = nullptr;
        bool got_frame = receiver_->get_latest_frame(&frame);

        if (state.cam_on == 1 && got_frame && frame != nullptr) {
            renderer_->render_frame(frame, state);
            av_frame_free(&frame);
        } else {
            if (frame != nullptr) {
                av_frame_free(&frame);
            }
            renderer_->render_frame(nullptr, state);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
    receiver_->stop();
}