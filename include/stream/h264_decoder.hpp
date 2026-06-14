/**
 * @file h264_decoder.hpp
 * @brief H.264データを元の生ピクセル（YUV420P）に解凍する部品
 */

 #ifndef H264_DECODER_HPP_
 #define H264_DECODER_HPP_
 
 #include <vector>
 #include <cstdint>
 
 extern "C" {
 #include <libavcodec/avcodec.h>
 }
 
 class H264Decoder {
 public:
     // 解凍後の「生のピクセルデータ」を安全に外に持ち出すための箱
     struct YuvFrame {
         uint8_t* y_data; int y_pitch; // Y（明るさ）のデータと1行のバイト数
         uint8_t* u_data; int u_pitch; // U（色差）のデータと1行のバイト数
         uint8_t* v_data; int v_pitch; // V（色差）のデータと1行のバイト数
         int width;
         int height;
     };
 
     H264Decoder();
     ~H264Decoder();
 
     // コピー禁止の安全装置
     H264Decoder(const H264Decoder&) = delete;
     H264Decoder& operator=(const H264Decoder&) = delete;
 
     // メイン処理：H.264データを入れると、解凍して out_frame に詰めて返してくれる
     bool decode_packet(const std::vector<uint8_t>& h264_data, YuvFrame& out_frame);
 
 private:
     AVCodecContext* codec_ctx_ = nullptr; // 解凍の「頭脳」
     AVFrame* frame_ = nullptr;            // 解凍後のデータが入る箱
     AVPacket* pkt_ = nullptr;             // 圧縮データを運び込むカプセル
 };
 
 #endif