#include "stream_app.hpp"
#include <iostream>
#include <chrono>

StreamApp::StreamApp(int port, const std::string& title, int width, int height, DecodeMode mode)
    : receiver_(port, mode), renderer_(title, width, height) {
}

void StreamApp::run(const std::atomic<bool>& keep_running) {
    std::cout << "画面の準備完了。映像の受信を待っています...\n" << std::endl;
    
    receiver_.start();
    AVFrame* current_frame = nullptr;

    // ★ ffplay化の心臓部：30fpsのペーシング（1フレームあたり33.3ミリ秒）
    const auto frame_duration = std::chrono::milliseconds(33); 
    auto next_render_time = std::chrono::steady_clock::now();

    while (keep_running) {
        if (!renderer_.poll_events()) {
            break;
        }

        auto now = std::chrono::steady_clock::now();
        
        // ★ 次のフレームを描画する時間が来たか？
        if (now >= next_render_time) {
            
            if (receiver_.get_latest_frame(&current_frame)) {
                // 時間通りに描画機へ！
                renderer_.render_frame(current_frame);
                av_frame_free(&current_frame);
                
                // 次の描画目標時間を33ms後に設定
                next_render_time += frame_duration;
                
                // ネットワークが途切れて時間が大きくズレた場合は、現在時刻にリセット（早送り現象の防止）
                if (now > next_render_time + frame_duration) {
                    next_render_time = now;
                }
            } else {
                // キューが空っぽ（パケット到着待ち）なので少し休む
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            
        } else {
            // まだ描画のタイミングではない（ペースを保つために休む）
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    std::cout << "\n終了シグナルを受信。システムを停止します..." << std::endl;
}