#include "stream/control_relay.hpp"
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

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
    
    // 状態記憶用の変数（whileループの外で保持）
    int prev_up = 0;
    int prev_down = 0;
    int target_speed = 0;

    while (keep_running_) {
        ssize_t len = recv(sock, buf, sizeof(buf), 0);
        if (len == 8) {
            {
                std::lock_guard<std::mutex> lock(mtx_);
                state_.steer = (buf[0] - 126.0f) / 126.0f;
                state_.throttle = (buf[1] - 126.0f) / 126.0f;
                state_.brake = (buf[2] - 126.0f) / 126.0f;
                state_.horn = buf[3];
                
                state_.cruise_set = buf[4];
                state_.cam_on = buf[5]; 

                int current_up = buf[6];
                int current_down = buf[7];

                // クルーズがONの時のみ、速度の変動を許可
                if (state_.cruise_set == 1) {
                    // 前回0(離されている)かつ今回1(押されている)の瞬間だけ反応（エッジ検出）
                    if (current_up == 1 && prev_up == 0) {
                        target_speed += 5;
                        if (target_speed > 30) {
                            target_speed = 30; // 上限を30km/hに制限
                        }
                    }
                    if (current_down == 1 && prev_down == 0) {
                        target_speed -= 5;
                        if (target_speed < 0) {
                            target_speed = 0; // 0未満にはならないように制限
                        }
                    }
                }

                // 今回のボタン状態を次回用に記憶
                prev_up = current_up;
                prev_down = current_down;

                // 画面表示用に構造体へセット
                state_.target_speed = target_speed;
            }
            
            // 車両側(Ras4)の負担を減らすため、ボタンの生データではなく計算済みの目標速度を統合して送る
            buf[6] = (unsigned char)target_speed;
            buf[7] = 0; // 統合したためDOWN用バッファは0でリセット
            
            sendto(sock, buf, len, 0, (struct sockaddr*)&target_addr, sizeof(target_addr));
        }
    }
    close(sock);
}