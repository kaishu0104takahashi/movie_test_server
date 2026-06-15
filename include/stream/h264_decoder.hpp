#ifndef H264_DECODER_HPP_
#define H264_DECODER_HPP_

#include <vector>
#include <cstdint>

extern "C" {
#include <libavcodec/avcodec.h>
}

// デコードモードの選択用スイッチ
enum class DecodeMode {
    Software_CPU,
    Hardware_Pi4
};

class H264Decoder {
public:
    struct YuvFrame {
        uint8_t* y_data; int y_pitch;
        uint8_t* u_data; int u_pitch;
        uint8_t* v_data; int v_pitch;
        int width;
        int height;
    };

    explicit H264Decoder(DecodeMode mode);
    ~H264Decoder();

    H264Decoder(const H264Decoder&) = delete;
    H264Decoder& operator=(const H264Decoder&) = delete;

    bool decode_packet(const std::vector<uint8_t>& h264_data, YuvFrame& out_frame);

private:
    AVCodecContext* codec_ctx_ = nullptr;
    AVFrame* frame_ = nullptr;
    AVPacket* pkt_ = nullptr;
};

#endif