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
    float throttle = 0.0f;
    float brake = 0.0f;
    int horn = 0;
    
    int cruise_set = 0;
    int cruise_off = 0;
    int cruise_speed_up = 0;
    int cruise_speed_down = 0;
    int target_speed = 0; 
    int cam_on = 0;

    int distance_alert = 0; // ★追加：障害物検知フラグ
};

class ControlRelay {
public:
    // ★追加: 距離センサの受信用ポート番号(local_dist_port)を引数に追加
    ControlRelay(int local_car_port, int local_cam_port, const std::string& target_ip, int target_car_port, int target_cam_port, int local_dist_port);
    ~ControlRelay();

    // 最新の操作状態を取得（描画スレッドから呼ばれる）
    ControlState get_current_state();

private:
    std::string target_ip_;
    int target_car_port_;
    int target_cam_port_;
    int local_dist_port_;

    std::atomic<bool> keep_running_{true};
    std::thread car_thread_;
    std::thread dist_thread_; // ★追加：距離フラグ受信スレッド
    
    ControlState state_;
    std::mutex mtx_;

    void car_relay_loop(int local_port);
    void dist_receive_loop(int local_port); // ★追加
};

#endif