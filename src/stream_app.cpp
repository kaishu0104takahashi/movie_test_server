#include "stream_app.hpp"
#include <iostream>
#include <chrono>

// 受信機(receiver_)と描画機(renderer_)を初期化するため
StreamApp::StreamApp(int port, const std::string& title, int width, int height, DecodeMode mode)
    : receiver_(port, mode), renderer_(title, width, height) {
}

// アプリケーションのメインループを実行するため
void StreamApp::run(const std::atomic<bool>& keep_running) {
    // 起動時の案内メッセージを出力するため
    std::cout << "画面の準備完了。映像の受信を待っています...\n" << std::endl;
    
    // パケットの受信スレッドを開始するため
    receiver_.start();
    
    // 描画するフレームを保持するポインタを初期化するため
    AVFrame* current_frame = nullptr;

    // 30fpsのペーシングを実現するため、1フレームあたりの時間(約33ms)を定義
    const auto frame_duration = std::chrono::milliseconds(33); 
    // 次回の描画目標時間を現在時刻で初期化するため
    auto next_render_time = std::chrono::steady_clock::now();

    // 終了シグナルが送られるまでループを継続するため
    while (keep_running) {
        // ウィンドウの×ボタンなどのイベントを処理し、終了要求があればループを抜けるため
        if (!renderer_.poll_events()) {
            break;
        }

        // 現在の時刻を取得するため
        auto now = std::chrono::steady_clock::now();
        
        // 描画の目標時刻に達しており、かつ最新フレームの取得に成功した場合のみ描画処理へ進むため
        if (now >= next_render_time && receiver_.get_latest_frame(&current_frame)) {
            
            // 取得したフレームを描画機に渡して画面を更新するため
            renderer_.render_frame(current_frame);
            
            // メモリリークを防ぐため、使用済みのフレームメモリを解放
            av_frame_free(&current_frame);
            
            // 次の描画目標時間を33ms後に設定するため
            next_render_time += frame_duration;
            
            // ネットワーク遅延などで時間が大きくズレた際、溜まった映像が一気に早送り再生されるのを防ぐため、
            // 目標時間を現在時刻にリセットして同期を修正
            if (now > next_render_time + frame_duration) {
                next_render_time = now;
            }
        } else {
            // 「まだ描画時刻に達していない」または「フレームが未到着」の場合は、CPU負荷を抑えるため1ms待機
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // ループを抜けた後、システム停止のメッセージを出力するため
    std::cout << "\n終了シグナルを受信。システムを停止します..." << std::endl;
}