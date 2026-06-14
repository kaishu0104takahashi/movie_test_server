#include "stream_app.hpp"
#include <iostream>
#include <chrono>

StreamApp::StreamApp(int port, const std::string& title, int width, int height)
    : receiver_(port), renderer_(title, width, height) {
}

void StreamApp::run(const std::atomic<bool>& keep_running) {
    std::cout << "画面の準備完了。映像の受信を待っています...\n" << std::endl;
    std::cout << "(終了するには画面の×ボタンか、ターミナルでCtrl+C)\n" << std::endl;

    // 裏方スレッド（受信・解凍）をスタート
    receiver_.start();

    // メインスレッド（描画）のループ
    while (keep_running) {
        // ① OSのイベント監視
        if (!renderer_.poll_events()) {
            break;
        }

        // ② 金庫から最新映像を取り出す
        if (receiver_.get_latest_frame(y_buf_, u_buf_, v_buf_, y_pitch_, u_pitch_, v_pitch_, width_, height_)) {
            // 描画機用の構造体にセット
            frame_.y_data = y_buf_.data(); frame_.y_pitch = y_pitch_;
            frame_.u_data = u_buf_.data(); frame_.u_pitch = u_pitch_;
            frame_.v_data = v_buf_.data(); frame_.v_pitch = v_pitch_;
            
            // ③ GPUで超高速描画
            renderer_.render_yuv(frame_);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    std::cout << "\n終了シグナルを受信。システムを停止します..." << std::endl;
    // receiver_ はクラスの破棄と同時に自動で安全に stop() されます（RAIIの力）
}