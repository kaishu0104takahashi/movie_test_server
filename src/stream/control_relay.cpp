#include "stream/control_relay.hpp"
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <algorithm> // std::max, std::min 用
#include <cmath>     // std::round 用に追加

ControlRelay::ControlRelay(int local_car_port, int local_cam_port, const std::string& target_ip, int target_car_port, int target_cam_port)
    : target_ip_(target_ip), target_car_port_(target_car_port), target_cam_port_(target_cam_port) {
    
    car_thread_ = std::thread(&ControlRelay::car_relay_loop, this, local_car_port);
}

ControlRelay::~ControlRelay() {
    keep_running_ = false;
    if (car_thread_.joinable()) car_thread_.join();
}

ControlState ControlRelay::get_current_state() {
    std::lock_guard<std::mutex> lock(mtx_);
    return state_;
}

void ControlRelay::car_relay_loop(int local_port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in local_addr{}, target_addr{};
    
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(local_port);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr));

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
        ssize_t len = recv(sock, buf, sizeof(buf), 0);
        if (len == 8) {
            {
                std::lock_guard<std::mutex> lock(mtx_);
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

                // --- クルーズON: 自動制御 ---
                if (state_.cruise_set == 1) {
                    if (current_up == 1 && prev_up == 0) {
                        target_speed += 5;
                        if (target_speed > 30) target_speed = 30; 
                    }
                    if (current_down == 1 && prev_down == 0) {
                        target_speed -= 5;
                        if (target_speed < 0) target_speed = 0; 
                    }

                    // 目標速度(0〜30)に合わせて、全閉(1.0)から上限(0.0: 半分)まで段階的に変化させる
                    final_throttle = 1.0f - (target_speed / 30.0f) * 1.0f;
                    pedal_lockout = false; // ON中はロック待機不要
                } 
                // --- クルーズOFF: 物理ペダル または 安全装置 ---
                else {
                    if (pedal_lockout) {
                        if (raw_throttle >= 0.9f) {
                            pedal_lockout = false; // ペダルが完全に離された(1.0付近)ら安全装置解除
                        } else {
                            final_throttle = 1.0f; // 離されるまでは強制的にアクセル全閉(1.0)を出力
                        }
                    }
                }

                // --- 安全なバイト変換（計算誤差による暴走防止） ---
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
            buf[6] = (unsigned char)target_speed;
            buf[7] = 0; 
            
            sendto(sock, buf, len, 0, (struct sockaddr*)&target_addr, sizeof(target_addr));
        }
    }
    close(sock);
}