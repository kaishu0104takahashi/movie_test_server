#include "stream/control_relay.hpp"
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

ControlRelay::ControlRelay(int local_car_port, int local_cam_port, const std::string& target_ip, int target_car_port, int target_cam_port)
    : target_ip_(target_ip), target_car_port_(target_car_port), target_cam_port_(target_cam_port) {
    
    car_thread_ = std::thread(&ControlRelay::car_relay_loop, this, local_car_port);
    cam_thread_ = std::thread(&ControlRelay::cam_relay_loop, this, local_cam_port);
}

ControlRelay::~ControlRelay() {
    keep_running_ = false;
    if (car_thread_.joinable()) car_thread_.join();
    if (cam_thread_.joinable()) cam_thread_.join();
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

    // タイムアウト設定 (100ms)
    struct timeval tv = {0, 100000};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(target_car_port_);
    inet_pton(AF_INET, target_ip_.c_str(), &target_addr.sin_addr);

    unsigned char buf[4];
    while (keep_running_) {
        ssize_t len = recv(sock, buf, sizeof(buf), 0);
        if (len == 4) {
            std::lock_guard<std::mutex> lock(mtx_);
            state_.steer = (buf[0] - 126.0f) / 126.0f;
            state_.throttle = (buf[1] - 126.0f) / 126.0f;
            state_.brake = (buf[2] - 126.0f) / 126.0f;
            state_.horn = buf[3];

            sendto(sock, buf, len, 0, (struct sockaddr*)&target_addr, sizeof(target_addr));
        }
    }
    close(sock);
}

void ControlRelay::cam_relay_loop(int local_port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in local_addr{}, target_addr{};
    
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(local_port);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr));

    struct timeval tv = {0, 100000};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(target_cam_port_);
    inet_pton(AF_INET, target_ip_.c_str(), &target_addr.sin_addr);

    unsigned char buf[1];
    while (keep_running_) {
        ssize_t len = recv(sock, buf, sizeof(buf), 0);
        if (len == 1) {
            std::lock_guard<std::mutex> lock(mtx_);
            state_.cam_on = buf[0];
            sendto(sock, buf, len, 0, (struct sockaddr*)&target_addr, sizeof(target_addr));
        }
    }
    close(sock);
}