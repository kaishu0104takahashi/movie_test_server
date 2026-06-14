/**
 * @file udp_receiver.hpp
 * @brief UDPで飛んできたMPEG-TSを受け止め、H.264データを抽出する部品
 */

 #ifndef UDP_RECEIVER_HPP_
 #define UDP_RECEIVER_HPP_
 
 #include <vector>
 #include <cstdint>
 #include <string>
 
 extern "C" {
 #include <libavformat/avformat.h>
 }
 
 class UdpReceiver {
 public:
     // ① 準備：待ち受けするポート番号（例: 1234）を指定する
     explicit UdpReceiver(int port);
     
     // ② 後片付け：ポートを閉じて通信を綺麗に終了する
     ~UdpReceiver();
 
     // コピー禁止のおまじない（通信ポートを扱うクラスの鉄則）
     UdpReceiver(const UdpReceiver&) = delete;
     UdpReceiver& operator=(const UdpReceiver&) = delete;
 
     // ③ メイン処理：ネットワークからH.264の塊を1つ受け取る
     bool receive_packet(std::vector<uint8_t>& out_h264_data);
 
 private:
     AVFormatContext* fmt_ctx_ = nullptr; // 受信と解体の「ルールブック」
     AVPacket* pkt_ = nullptr;            // 受け取ったデータを入れる「カプセル」
     int video_stream_index_ = -1;        // 映像データが流れているレーンの番号
 };
 
 #endif