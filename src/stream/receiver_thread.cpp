#include "stream/receiver_thread.hpp"
#include <iostream>

ReceiverThread::ReceiverThread(int port, DecodeMode mode) : port_(port), mode_(mode) {}

ReceiverThread::~ReceiverThread() {
    stop();
    std::lock_guard<std::mutex> lock(mutex_);
    while (!frame_queue_.empty()) {
        AVFrame* f = frame_queue_.front();
        frame_queue_.pop();
        av_frame_free(&f);
    }
}

void ReceiverThread::start() {
    stop_flag_.store(false);
    active_.store(true);
    worker_ = std::thread(&ReceiverThread::thread_loop, this);
}

void ReceiverThread::stop() {
    stop_flag_.store(true);
    if (worker_.joinable()) worker_.detach();
}

void ReceiverThread::set_active(bool active) {
    active_.store(active);
    
    // 非アクティブ（OFF）になった瞬間にキューをクリアして古い映像を捨てる
    if (!active) {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!frame_queue_.empty()) {
            AVFrame* f = frame_queue_.front();
            frame_queue_.pop();
            av_frame_free(&f);
        }
    }
}

bool ReceiverThread::get_latest_frame(AVFrame** out_frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (frame_queue_.empty()) return false;

    *out_frame = frame_queue_.front();
    frame_queue_.pop();
    return true;
}

void ReceiverThread::thread_loop() {
    try {
        UdpReceiver receiver(port_);
        H264Decoder decoder(mode_);

        AVPacket* pkt = av_packet_alloc();
        AVFrame* decoded_frame = av_frame_alloc();

        while (!stop_flag_.load(std::memory_order_relaxed)) {
            if (receiver.receive_packet(pkt)) {
                
                // active_ が true の時だけデコード処理を行う
                if (active_.load(std::memory_order_relaxed)) {
                    if (decoder.send_packet(pkt)) {
                        while (decoder.receive_frame(decoded_frame)) {
                            std::lock_guard<std::mutex> lock(mutex_);
                            frame_queue_.push(av_frame_clone(decoded_frame));

                            // バッファが溜まりすぎたら古いものを捨てる
                            if (frame_queue_.size() > 5) {
                                AVFrame* old = frame_queue_.front();
                                frame_queue_.pop();
                                av_frame_free(&old);
                            }
                        }
                    }
                }
                // パケットはデコードの有無に関わらず解放し、バッファ詰まりを防ぐ
                av_packet_unref(pkt);
            }
        }
        av_frame_free(&decoded_frame);
        av_packet_free(&pkt);

    } catch (const std::exception& e) {
        std::cerr << "[裏方スレッド エラー] " << e.what() << std::endl;
    }
}