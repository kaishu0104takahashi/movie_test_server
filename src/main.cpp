#include <iostream>
#include <csignal>
#include <atomic>
#include "stream_app.hpp"

// メインスレッドや別スレッドから安全に参照・変更できる終了フラグを定義するため
std::atomic<bool> keep_running(true);

// Ctrl+C (SIGINT) などが入力された際に、システムを安全に停止させるため
void signal_handler(int) {
    keep_running = false;
}

int main() {
    // OSからの割り込みシグナルを捕捉し、自前の終了処理に紐付けるため
    std::signal(SIGINT, signal_handler);
    
    // 起動時の案内メッセージをコンソールに出力するため
    std::cout << "--- 映像伝送 Server (受信側) 起動 ---" << std::endl;
    
    try {
        // CPUの4コアを全開にして処理を行う超低遅延モードを設定するため
        // （GPUエコモードを使用する場合は DecodeMode::Hardware_Pi4 に変更）
        DecodeMode mode = DecodeMode::Software_CPU;

        // アプリケーションのインスタンスを生成し、受信ポート・ウィンドウ名・解像度を設定するため
        // （Client側の出力解像度に合わせて 1920x1080 を指定）
        // StreamApp app(1234, "EV Camera Stream", 640, 480, mode); // 低解像度・最適化用
        // StreamApp app(1234, "EV Camera Stream", 1600, 896, mode); // 中解像度用
        //StreamApp app(1234, "EV Camera Stream", 1920, 1080, mode);
        //StreamApp app(1234, "EV Camera Stream", 1280, 720, mode);
        //StreamApp app(1234, "EV Camera Stream", 800, 600, mode);
        //StreamApp app(1234, "EV Camera Stream", 640, 480, mode);
        StreamApp app(1234, "EV Camera Stream", 320, 240, mode);

        // 終了フラグを渡し、アプリケーションのメインループを実行するため
        app.run(keep_running);

    } catch (const std::exception& e) {
        // 実行中に予期せぬ致命的なエラーが発生した場合、内容を出力して異常終了させるため
        std::cerr << "\n[致命的なエラー] " << e.what() << std::endl;
        return -1;
    }

    // エラーなくメインループを抜けた場合、正常終了とするため
    return 0;
}