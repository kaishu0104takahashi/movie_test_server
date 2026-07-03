#ifndef H264_DECODER_HPP_
#define H264_DECODER_HPP_

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

    // ★ デコーダも送受信を完全に分離する
    bool send_packet(AVPacket* pkt);
    bool receive_frame(AVFrame* out_frame);

private:
    AVCodecContext* codec_ctx_ = nullptr;
};

#endif