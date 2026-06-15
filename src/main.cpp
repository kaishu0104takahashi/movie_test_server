#include <iostream>
#include <csignal>
#include <atomic>
#include "stream_app.hpp"

std::atomic<bool> keep_running(true);
void signal_handler(int) {
    keep_running = false;
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::cout << "--- 映像伝送 Server (受信側) 起動 ---" << std::endl;

    try {
        // ★ここでモードを指定！
        // DecodeMode::Software_CPU : CPU4コア全開の超低遅延モード
        // DecodeMode::Hardware_Pi4 : GPU専用チップのエコモード
        DecodeMode mode = DecodeMode::Hardware_Pi4;

        // 解像度はClient側に合わせてください（例: 320, 240 または 640, 360など）
        StreamApp app(1234, "EV Camera Stream", 320, 240, mode);

        app.run(keep_running);

    } catch (const std::exception& e) {
        std::cerr << "\n[致命的なエラー] " << e.what() << std::endl;
        return -1;
    }

    return 0;
}