#ifndef RECEIVER_THREAD_HPP_
#define RECEIVER_THREAD_HPP_

#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <queue> // ★ffplayの滑らかさを生む「キュー」

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

    bool get_latest_frame(AVFrame** out_frame);

private:
    int port_;
    DecodeMode mode_;
    std::thread worker_;
    std::atomic<bool> stop_flag_{false};

    std::mutex mutex_;
    
    // ★ 単一フレームから「順番待ち列（FIFO）」に変更
    std::queue<AVFrame*> frame_queue_; 

    void thread_loop();
};

#endif