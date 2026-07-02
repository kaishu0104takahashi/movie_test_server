/**
 * @file h264_decoder.hpp
 * @brief H.264データを元の生ピクセルに解凍する部品
 */
#ifndef H264_DECODER_HPP_
#define H264_DECODER_HPP_

#include <vector>
#include <cstdint>

extern "C" {
#include <libavcodec/avcodec.h>
}

enum class DecodeMode {
    Software_CPU,
    Hardware_Pi4
};

class H264Decoder {
public:
    explicit H264Decoder(DecodeMode mode);
    ~H264Decoder();

    H264Decoder(const H264Decoder&) = delete;
    H264Decoder& operator=(const H264Decoder&) = delete;

    // AVFrameに直接解凍結果を書き込むように変更
    bool decode_packet(const std::vector<uint8_t>& h264_data, AVFrame* out_frame);

private:
    AVCodecContext* codec_ctx_ = nullptr;
    AVPacket* pkt_ = nullptr;
};

#endif