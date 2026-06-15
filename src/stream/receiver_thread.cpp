#include "stream/receiver_thread.hpp"
#include <iostream>

ReceiverThread::ReceiverThread(int port, DecodeMode mode) : port_(port), mode_(mode) {}

ReceiverThread::~ReceiverThread() {
    stop();
}

void ReceiverThread::start() {
    stop_flag_.store(false);
    worker_ = std::thread(&ReceiverThread::thread_loop, this);
}

void ReceiverThread::stop() {
    stop_flag_.store(true);
    if (worker_.joinable()) {
        worker_.detach(); 
    }
}

bool ReceiverThread::get_latest_frame(std::vector<uint8_t>& y, std::vector<uint8_t>& u, std::vector<uint8_t>& v,
                                      int& y_pitch, int& u_pitch, int& v_pitch, int& width, int& height) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!has_new_frame_) return false;

    y = y_buf_; u = u_buf_; v = v_buf_;
    y_pitch = y_pitch_; u_pitch = u_pitch_; v_pitch = v_pitch_;
    width = width_; height = height_;
    
    has_new_frame_ = false;
    return true;
}

void ReceiverThread::thread_loop() {
    try {
        std::cout << "[裏方スレッド] 受信機と解凍機を準備中..." << std::endl;
        UdpReceiver receiver(port_);
        // モードを渡して解凍機を生成！
        H264Decoder decoder(mode_);

        std::vector<uint8_t> h264_data;
        H264Decoder::YuvFrame frame;

        std::cout << "[裏方スレッド] >>> パケット受信待機中 (Port: " << port_ << ") <<<" << std::endl;

        while (!stop_flag_.load(std::memory_order_relaxed)) {
            if (receiver.receive_packet(h264_data)) {
                if (decoder.decode_packet(h264_data, frame)) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    
                    size_t y_size = frame.y_pitch * frame.height;
                    size_t u_size = frame.u_pitch * (frame.height / 2);
                    size_t v_size = frame.v_pitch * (frame.height / 2);
                    
                    y_buf_.assign(frame.y_data, frame.y_data + y_size);
                    u_buf_.assign(frame.u_data, frame.u_data + u_size);
                    v_buf_.assign(frame.v_data, frame.v_data + v_size);
                    
                    y_pitch_ = frame.y_pitch;
                    u_pitch_ = frame.u_pitch;
                    v_pitch_ = frame.v_pitch;
                    width_ = frame.width;
                    height_ = frame.height;
                    
                    has_new_frame_ = true;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[裏方スレッド エラー] " << e.what() << std::endl;
    }
}