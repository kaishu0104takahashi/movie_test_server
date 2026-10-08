#include "stream/control_relay.hpp"
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

ControlRelay::ControlRelay(int local_car_port, int local_cam_port, const std::string& target_ip, int target_car_port, int target_cam_port, int local_dist_port)
    : target_ip_(target_ip), target_car_port_(target_car_port), target_cam_port_(target_cam_port), local_dist_port_(local_dist_port) {
    
    // ★追加: 起動時の時間を初期値としてセット
    last_cockpit_recv_time_ = std::chrono::steady_clock::now();
    last_client_recv_time_ = std::chrono::steady_clock::now();

    car_thread_ = std::thread(&ControlRelay::car_relay_loop, this, local_car_port);
    
    // ★追加: 3000番ポートでの受信スレッドを起動
    dist_thread_ = std::thread(&ControlRelay::dist_receive_loop, this, local_dist_port);
}

ControlRelay::~ControlRelay() {
    keep_running_ = false;
    if (car_thread_.joinable()) car_thread_.join();
    if (dist_thread_.joinable()) dist_thread_.join(); // ★追加
}

ControlState ControlRelay::get_current_state() {
    std::lock_guard<std::mutex> lock(mtx_);
    
    // ★追加: 最終受信時刻から1秒(1000ms)以内なら接続中(true)と判定
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

    struct timeval tv = {0, 100000};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(target_car_port_);
    inet_pton(AF_INET, target_ip_.c_str(), &target_addr.sin_addr);

    unsigned char buf[8]; 
    
    int prev_up = 0;
    int prev_down = 0;
    int target_speed = 0;

    while (keep_running_) {
        ssize_t len = recv(sock, buf, sizeof(buf), 0);
        if (len == 8) {
            {
                std::lock_guard<std::mutex> lock(mtx_);
                // ★追加: コックピットからデータを受信したら現在時刻に更新
                last_cockpit_recv_time_ = std::chrono::steady_clock::now();

                state_.steer = (buf[0] - 126.0f) / 126.0f;
                state_.throttle = (buf[1] - 126.0f) / 126.0f;
                state_.brake = (buf[2] - 126.0f) / 126.0f;
                state_.horn = buf[3];
                
                state_.cruise_set = buf[4];
                state_.cam_on = buf[5]; 

                int current_up = buf[6];
                int current_down = buf[7];

                if (state_.cruise_set == 1) {
                    if (current_up == 1 && prev_up == 0) {
                        target_speed += 5;
                    }
                    if (current_down == 1 && prev_down == 0) {
                        target_speed -= 5;
                        if (target_speed < 0) {
                            target_speed = 0;
                        }
                    }
                }

                prev_up = current_up;
                prev_down = current_down;

                state_.target_speed = target_speed;
            }
            
            buf[6] = (unsigned char)target_speed;
            buf[7] = 0; 
            
            sendto(sock, buf, len, 0, (struct sockaddr*)&target_addr, sizeof(target_addr));
        }
    }
    close(sock);
}

// ★追加: クライアントからの3000番ポートを受信するループ
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
            // ★追加: クライアント(rpi5-client)からデータを受信したら現在時刻に更新（生存確認）
            last_client_recv_time_ = std::chrono::steady_clock::now();
            state_.distance_alert = buf[0];
        }
    }
    close(sock);
}