#include "stream/h264_decoder.hpp"
#include <stdexcept>
#include <iostream>

H264Decoder::H264Decoder(DecodeMode mode) {
    const AVCodec* codec = nullptr;
    if (mode == DecodeMode::Hardware_Pi4) {
        codec = avcodec_find_decoder_by_name("h264_v4l2m2m");
        if (!codec) throw std::runtime_error("エラー: ハードウェアデコーダが見つかりません");
    } else {
        codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (!codec) throw std::runtime_error("エラー: ソフトウェアデコーダが見つかりません");
    }

    codec_ctx_ = avcodec_alloc_context3(codec);
    if (!codec_ctx_) {
        throw std::runtime_error("エラー: デコーダの設定データを作れません");
    }

    if (mode == DecodeMode::Software_CPU) {
        codec_ctx_->flags |= AV_CODEC_FLAG_LOW_DELAY;
        
        // エラー時の挙動を一番調子が良かった元の設定に戻す
        codec_ctx_->flags |= AV_CODEC_FLAG_OUTPUT_CORRUPT;
        codec_ctx_->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;

        codec_ctx_->thread_type = FF_THREAD_SLICE;
        codec_ctx_->thread_count = 4;
    }

    if (avcodec_open2(codec_ctx_, codec, nullptr) < 0) {
        throw std::runtime_error("エラー: デコーダの起動に失敗しました");
    }

    pkt_ = av_packet_alloc();
}

H264Decoder::~H264Decoder() {
    if (pkt_) av_packet_free(&pkt_);
    if (codec_ctx_) avcodec_free_context(&codec_ctx_);
}

bool H264Decoder::decode_packet(const std::vector<uint8_t>& h264_data, AVFrame* out_frame) {
    if (h264_data.empty()) return false;

    pkt_->data = const_cast<uint8_t*>(h264_data.data());
    pkt_->size = h264_data.size();

    int ret = avcodec_send_packet(codec_ctx_, pkt_);
    if (ret < 0) return false;

    if (avcodec_receive_frame(codec_ctx_, out_frame) == 0) {
        return true;
    }

    return false;
}