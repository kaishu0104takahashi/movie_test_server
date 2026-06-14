#include "stream/receiver_thread.hpp"
#include <iostream>

ReceiverThread::ReceiverThread(int port) : port_(port) {}

ReceiverThread::~ReceiverThread() {
    stop();
}

void ReceiverThread::start() {
    stop_flag_.store(false);
    worker_ = std::thread(&ReceiverThread::thread_loop, this);
}

void ReceiverThread::stop() {
    stop_flag_.store(true);
    // スレッドが安全に終了するまで待つ
    // ※今回はUDPの受信待ち（ブロック）で止まる可能性があるのでデタッチで強制終了させる安全策を取ります
    if (worker_.joinable()) {
        worker_.detach(); 
    }
}

bool ReceiverThread::get_latest_frame(std::vector<uint8_t>& y, std::vector<uint8_t>& u, std::vector<uint8_t>& v,
                                      int& y_pitch, int& u_pitch, int& v_pitch, int& width, int& height) {
    // 金庫の鍵をかける（読み取り中に裏方が書き込まないようにする）
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!has_new_frame_) return false;

    // データを外に渡す
    y = y_buf_; u = u_buf_; v = v_buf_;
    y_pitch = y_pitch_; u_pitch = u_pitch_; v_pitch = v_pitch_;
    width = width_; height = height_;
    
    has_new_frame_ = false; // 取り出したらフラグを下ろす
    return true;
}

void ReceiverThread::thread_loop() {
    try {
        std::cout << "[裏方スレッド] 受信機と解凍機を準備中..." << std::endl;
        UdpReceiver receiver(port_);
        H264Decoder decoder;

        std::vector<uint8_t> h264_data;
        H264Decoder::YuvFrame frame;

        std::cout << "[裏方スレッド] >>> パケット受信待機中 (Port: " << port_ << ") <<<" << std::endl;

        while (!stop_flag_.load(std::memory_order_relaxed)) {
            // 1. パケットを受け取る（届くまでここで待機します）
            if (receiver.receive_packet(h264_data)) {
                
                // 2. 受け取ったH.264を解凍する
                if (decoder.decode_packet(h264_data, frame)) {
                    
                    // 3. 解凍成功！金庫に鍵をかけてデータをコピーする
                    std::lock_guard<std::mutex> lock(mutex_);
                    
                    size_t y_size = frame.y_pitch * frame.height;
                    size_t u_size = frame.u_pitch * (frame.height / 2);
                    size_t v_size = frame.v_pitch * (frame.height / 2);
                    
                    // 生のピクセルデータを安全な配列にコピー
                    y_buf_.assign(frame.y_data, frame.y_data + y_size);
                    u_buf_.assign(frame.u_data, frame.u_data + u_size);
                    v_buf_.assign(frame.v_data, frame.v_data + v_size);
                    
                    y_pitch_ = frame.y_pitch;
                    u_pitch_ = frame.u_pitch;
                    v_pitch_ = frame.v_pitch;
                    width_ = frame.width;
                    height_ = frame.height;
                    
                    has_new_frame_ = true; // 「新しい映像が入ったよ」と合図
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[裏方スレッド エラー] " << e.what() << std::endl;
    }
}