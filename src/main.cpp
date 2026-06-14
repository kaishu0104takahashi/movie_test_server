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
        // ① アプリケーション全体を生成（ポート番号、タイトル、幅、高さを指定）
        StreamApp app(1234, "EV Camera Stream", 320, 240);

        // ② アプリケーションを実行！（安全装置の変数を渡す）
        app.run(keep_running);

    } catch (const std::exception& e) {
        std::cerr << "\n[致命的なエラー] " << e.what() << std::endl;
        return -1;
    }

    return 0;
}