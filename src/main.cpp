#include <iostream>
#include <csignal>
#include <atomic>
#include "stream_app.hpp"

// メインスレッド終了用の安全装置
std::atomic<bool> keep_running(true);
void signal_handler(int) {
    keep_running = false;
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::cout << "--- 映像伝送 Server (受信側) 起動 ---" << std::endl;
    
    try {
        // =================================================================================
        // これにより h264_decoder.cpp 内部で、自動的にCPU4コアをフル活用する超低遅延設定
        // DecodeMode::Software_CPU : CPU4コア全開の超低遅延モード
        // DecodeMode::Hardware_Pi4 : GPU専用チップのエコモード
    
        DecodeMode mode = DecodeMode::Software_CPU;

        // 解像度をClient側（640x360）と同期
        StreamApp app(1234, "EV Camera Stream", 640, 480, mode);
        
        // アプリケーションを実行
        app.run(keep_running);

    } catch (const std::exception& e) {
        std::cerr << "\n[致命的なエラー] " << e.what() << std::endl;
        return -1;
    }

    return 0;
}