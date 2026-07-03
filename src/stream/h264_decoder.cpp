#include "stream/h264_decoder.hpp"
#include <stdexcept>

H264Decoder::H264Decoder(DecodeMode mode) {
    const AVCodec* codec = nullptr;
    if (mode == DecodeMode::Hardware_Pi4) {
        codec = avcodec_find_decoder_by_name("h264_v4l2m2m");
    } else {
        codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    }

    if (!codec) throw std::runtime_error("デコーダが見つかりません");

    codec_ctx_ = avcodec_alloc_context3(codec);
    if (!codec_ctx_) throw std::runtime_error("設定データを作れません");

    if (mode == DecodeMode::Software_CPU) {
        codec_ctx_->flags |= AV_CODEC_FLAG_LOW_DELAY;
        codec_ctx_->flags |= AV_CODEC_FLAG_OUTPUT_CORRUPT;
        codec_ctx_->error_concealment = FF_EC_DEBLOCK;
        codec_ctx_->thread_type = FF_THREAD_SLICE;
        codec_ctx_->thread_count = 4;
    }

    if (avcodec_open2(codec_ctx_, codec, nullptr) < 0) {
        throw std::runtime_error("デコーダの起動に失敗");
    }
}

H264Decoder::~H264Decoder() {
    if (codec_ctx_) avcodec_free_context(&codec_ctx_);
}

bool H264Decoder::send_packet(AVPacket* pkt) {
    int ret = avcodec_send_packet(codec_ctx_, pkt);
    return (ret == 0);
}

bool H264Decoder::receive_frame(AVFrame* out_frame) {
    int ret = avcodec_receive_frame(codec_ctx_, out_frame);
    return (ret == 0);
}