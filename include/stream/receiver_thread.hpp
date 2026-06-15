#ifndef RECEIVER_THREAD_HPP_
#define RECEIVER_THREAD_HPP_

#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

#include "stream/udp_receiver.hpp"
#include "stream/h264_decoder.hpp"

class ReceiverThread {
public:
    ReceiverThread(int port, DecodeMode mode);
    ~ReceiverThread();

    ReceiverThread(const ReceiverThread&) = delete;
    ReceiverThread& operator=(const ReceiverThread&) = delete;

    void start();
    void stop();

    bool get_latest_frame(std::vector<uint8_t>& y, std::vector<uint8_t>& u, std::vector<uint8_t>& v,
                          int& y_pitch, int& u_pitch, int& v_pitch, int& width, int& height);

private:
    int port_;
    DecodeMode mode_;
    std::thread worker_;
    std::atomic<bool> stop_flag_{false};

    std::mutex mutex_;
    bool has_new_frame_ = false;
    
    std::vector<uint8_t> y_buf_, u_buf_, v_buf_;
    int y_pitch_ = 0, u_pitch_ = 0, v_pitch_ = 0;
    int width_ = 0, height_ = 0;

    void thread_loop();
};

#endif