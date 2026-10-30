#include "stream/control_relay.hpp"
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <algorithm> // std::max, std::min 用
#include <cmath>     // std::round 用

ControlRelay::ControlRelay(int local_car_port, int local_cam_port, const std::string& target_ip, int target_car_port, int target_cam_port, int local_dist_port)
    : target_ip_(target_ip), target_car_port_(target_car_port), target_cam_port_(target_cam_port), local_dist_port_(local_dist_port) {
    
    last_cockpit_recv_time_ = std::chrono::steady_clock::now();
    last_client_recv_time_ = std::chrono::steady_clock::now();

    car_thread_ = std::thread(&ControlRelay::car_relay_loop, this, local_car_port);
    dist_thread_ = std::thread(&ControlRelay::dist_receive_loop, this, local_dist_port);
}

ControlRelay::~ControlRelay() {
    keep_running_ = false;
    if (car_thread_.joinable()) car_thread_.join();
    if (dist_thread_.joinable()) dist_thread_.join();
}

ControlState ControlRelay::get_current_state() {
    std::lock_guard<std::mutex> lock(mtx_);
    
    auto now = std::chrono::steady_clock::now();
    state_.cockpit_connected = (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_cockpit_recv_time_).count() < 1000);
    state_.client_connected = (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_client_recv_time_).count() < 1000);

    return state_;
}

void ControlRelay::car_relay_loop(int local_port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in local_addr{}, target_addr{};
    
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(local_port);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr));

    // タイムアウト設定 (100ms)
    struct timeval tv = {0, 100000};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(target_car_port_);
    inet_pton(AF_INET, target_ip_.c_str(), &target_addr.sin_addr);

    unsigned char buf[8]; 
    
    // 状態記憶用の変数
    int prev_up = 0;
    int prev_down = 0;
    int target_speed = 0;
    bool pedal_lockout = false; // クルーズ解除時の急発進防止フラグ

    while (keep_running_) {
        // recvが100msでタイムアウトしてループが回り続ける
        ssize_t len = recv(sock, buf, sizeof(buf), 0);
        auto now = std::chrono::steady_clock::now();

        if (len == 8) {
            {
                std::lock_guard<std::mutex> lock(mtx_);
                // データを受信したら現在時刻に更新
                last_cockpit_recv_time_ = now;

                state_.steer = (buf[0] - 126.0f) / 126.0f;
                // ペダルの初期値(全閉)は 1.0、最大踏み込みは -1.0
                float raw_throttle = (buf[1] - 126.0f) / 126.0f;
                state_.brake = (buf[2] - 126.0f) / 126.0f;
                state_.horn = buf[3];

                int current_cruise = buf[4];
                int current_up = buf[6];
                int current_down = buf[7];

                // クルーズがONからOFFに切り替わった瞬間に安全装置を作動
                if (state_.cruise_set == 1 && current_cruise == 0) {
                    pedal_lockout = true;
                    target_speed = 0;
                }

                state_.cruise_set = current_cruise;
                state_.cam_on = buf[5]; 

                float final_throttle = raw_throttle;

                // --- クルーズON: 自動制御（物理アクセルの値は送らず、目標速度に対応した値を送る） ---
                if (state_.cruise_set == 1) {
                    if (current_up == 1 && prev_up == 0) {
                        target_speed += 5;
                        if (target_speed > 30) target_speed = 30; // 上限30km/h
                    }
                    if (current_down == 1 && prev_down == 0) {
                        target_speed -= 5;
                        if (target_speed < 0) target_speed = 0;   // 下限0km/h
                    }

                    // 目標速度(0〜30km/h)に合わせて、全閉(1.0)から上限(0.0: 半分)まで段階的に変化させる
                    final_throttle = 1.0f - (target_speed / 30.0f) * 1.0f;
                    pedal_lockout = false; // ON中はロック待機不要
                } 
                // --- クルーズOFF: 物理ペダル または 安全装置 ---
                else {
                    target_speed = 0;
                    if (pedal_lockout) {
                        if (raw_throttle >= 0.9f) {
                            pedal_lockout = false; // ペダルが完全に離された(1.0付近)ら安全装置解除
                        } else {
                            final_throttle = 1.0f; // 離されるまでは強制的にアクセル全閉(1.0)を出力
                        }
                    }
                }

                // --- 安全なバイト変換（計算されたアクセル値で buf[1] を上書き） ---
                int throt_val = static_cast<int>(std::round(126.0f + final_throttle * 126.0f));
                buf[1] = static_cast<unsigned char>(std::max(0, std::min(252, throt_val)));

                // 今回のボタン状態を次回用に記憶
                prev_up = current_up;
                prev_down = current_down;

                // 画面表示用に構造体へセット
                state_.throttle = final_throttle;
                state_.target_speed = target_speed;
            }
            
            // 目標速度の統合とリセット
            buf[6] = static_cast<unsigned char>(target_speed);
            buf[7] = 0; 
            
            // 車両側(rpi5-client)へ送信
            sendto(sock, buf, len, 0, (struct sockaddr*)&target_addr, sizeof(target_addr));
        }

        // ==============================================================
        // コックピットの通信切断（1秒以上途絶）を検知した場合のフェールセーフ
        // ==============================================================
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_cockpit_recv_time_).count() >= 1000) {
            unsigned char safe_buf[8];
            safe_buf[0] = 126; // ステアリング中央
            safe_buf[1] = 252; // アクセル全閉
            safe_buf[2] = 252; // ブレーキ全閉
            safe_buf[3] = 0;   // ホーンOFF
            safe_buf[4] = 0;   // クルーズOFF
            safe_buf[5] = 0;   // カメラOFF
            safe_buf[6] = 0;   // 目標速度 0km/h
            safe_buf[7] = 0;   // 予約枠

            {
                std::lock_guard<std::mutex> lock(mtx_);
                // 内部状態とサーバー画面のUI表示も安全状態にリセット
                target_speed = 0;
                prev_up = 0;
                prev_down = 0;
                pedal_lockout = false;

                state_.steer = 0.0f;
                state_.throttle = 1.0f;
                state_.brake = 1.0f;
                state_.horn = 0;
                state_.cruise_set = 0;
                state_.cam_on = 0;
                state_.target_speed = 0;
            }
            
            // rpi5-clientへ「カメラOFF・初期状態」のパケットを送り続ける（約100ms間隔）
            sendto(sock, safe_buf, sizeof(safe_buf), 0, (struct sockaddr*)&target_addr, sizeof(target_addr));
        }
    }
    close(sock);
}

void ControlRelay::dist_receive_loop(int local_port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in local_addr{};
    
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(local_port);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr));

    struct timeval tv = {0, 100000};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    unsigned char buf[1];
    while (keep_running_) {
        ssize_t len = recv(sock, buf, sizeof(buf), 0);
        if (len == 1) {
            std::lock_guard<std::mutex> lock(mtx_);
            // クライアント(rpi5-client)からデータを受信したら現在時刻に更新
            last_client_recv_time_ = std::chrono::steady_clock::now();
            state_.distance_alert = buf[0];
        }
    }
    close(sock);
}