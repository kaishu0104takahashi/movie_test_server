#ifndef CONTROL_RELAY_HPP_
#define CONTROL_RELAY_HPP_

#include <thread>
#include <atomic>
#include <mutex>
#include <string>
#include <netinet/in.h>

// UI描画用に保持する操作データの構造体
struct ControlState {
    float steer = 0.0f;
    float throttle = -1.0f;
    float brake = -1.0f;
    int horn = 0;
    int cam_on = 0;
};

class ControlRelay {
public:
    ControlRelay(int local_car_port, int local_cam_port, const std::string& target_ip, int target_car_port, int target_cam_port);
    ~ControlRelay();

    // 最新の操作状態を取得（描画スレッドから呼ばれる）
    ControlState get_current_state();

private:
    std::string target_ip_;
    int target_car_port_;
    int target_cam_port_;

    std::atomic<bool> keep_running_{true};
    std::thread car_thread_;
    std::thread cam_thread_;
    
    ControlState state_;
    std::mutex mtx_;

    void car_relay_loop(int local_port);
    void cam_relay_loop(int local_port);
};

#endif