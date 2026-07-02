#include "stream/receiver_thread.hpp"
#include <iostream>

ReceiverThread::ReceiverThread(int port, DecodeMode mode) : port_(port), mode_(mode) {
}

ReceiverThread::~ReceiverThread() {
    stop();
    // キューに残ったフレームをすべて綺麗に掃除
    std::lock_guard<std::mutex> lock(mutex_);
    while (!frame_queue_.empty()) {
        AVFrame* f = frame_queue_.front();
        frame_queue_.pop();
        av_frame_free(&f);
    }
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

bool ReceiverThread::get_latest_frame(AVFrame** out_frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (frame_queue_.empty()) return false;

    // ★ キューの先頭（一番古いフレーム）から順番に取り出す
    *out_frame = frame_queue_.front();
    frame_queue_.pop();
    
    return true;
}

void ReceiverThread::thread_loop() {
    try {
        std::cout << "[裏方スレッド] 受信機と解凍機を準備中..." << std::endl;
        UdpReceiver receiver(port_);
        H264Decoder decoder(mode_);

        std::vector<uint8_t> h264_data;
        AVFrame* decoded_frame = av_frame_alloc();

        std::cout << "[裏方スレッド] >>> パケット受信待機中 (Port: " << port_ << ") <<<" << std::endl;

        while (!stop_flag_.load(std::memory_order_relaxed)) {
            if (receiver.receive_packet(h264_data)) {
                if (decoder.decode_packet(h264_data, decoded_frame)) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    
                    // ★ 上書きではなく、列の最後尾に追加（クローン）する
                    frame_queue_.push(av_frame_clone(decoded_frame));
                    
                    // ★ 遠隔操作の命「超低遅延」を守るための安全装置
                    // ネットワークが詰まってバッファが溜まりすぎた場合、一番古いものを捨てて
                    // 遅延を最大約160ms（5フレーム）以内に強制維持する
                    if (frame_queue_.size() > 5) {
                        AVFrame* old = frame_queue_.front();
                        frame_queue_.pop();
                        av_frame_free(&old);
                    }
                }
            }
        }
        av_frame_free(&decoded_frame);
    } catch (const std::exception& e) {
        std::cerr << "[裏方スレッド エラー] " << e.what() << std::endl;
    }
}