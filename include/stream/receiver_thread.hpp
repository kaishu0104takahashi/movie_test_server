/**
 * @file receiver_thread.hpp
 * @brief パケット受信とH.264解凍を裏方で回し続ける司令塔
 */

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
     ReceiverThread(int port);
     ~ReceiverThread();
 
     // コピー禁止
     ReceiverThread(const ReceiverThread&) = delete;
     ReceiverThread& operator=(const ReceiverThread&) = delete;
 
     void start();
     void stop();
 
     // 描画用のメインスレッドが、最新のフレームを取り出すための関数
     // （新しい映像が届いていれば true を返す）
     bool get_latest_frame(std::vector<uint8_t>& y, std::vector<uint8_t>& u, std::vector<uint8_t>& v,
                           int& y_pitch, int& u_pitch, int& v_pitch, int& width, int& height);
 
 private:
     int port_;
     std::thread worker_;
     std::atomic<bool> stop_flag_{false};
 
     // --- スレッド間通信用（金庫） ---
     std::mutex mutex_; // 同時に触らないようにする「鍵」
     bool has_new_frame_ = false;
     
     // 金庫の中身（解凍されたYUVデータ）
     std::vector<uint8_t> y_buf_, u_buf_, v_buf_;
     int y_pitch_ = 0, u_pitch_ = 0, v_pitch_ = 0;
     int width_ = 0, height_ = 0;
 
     void thread_loop();
 };
 
 #endif