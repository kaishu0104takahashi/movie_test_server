#include "stream/h264_decoder.hpp"
#include <stdexcept>
#include <iostream>

H264Decoder::H264Decoder() {
    // 1. H.264の解凍機を探す
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        throw std::runtime_error("エラー: H.264デコーダが見つかりません");
    }

    // 2. 解凍機の設定データを作成
    codec_ctx_ = avcodec_alloc_context3(codec);
    if (!codec_ctx_) {
        throw std::runtime_error("エラー: デコーダの設定データを作れません");
    }

    // =======================================================
    // ★ 遠隔操作EV用：超低遅延＆マルチスレッド解凍マジック
    // =======================================================
    // 映像を溜め込まず、届いた瞬間に解凍する！
    codec_ctx_->flags |= AV_CODEC_FLAG_LOW_DELAY; 
    // フレーム単位ではなく「スライス（画像の切れ端）」単位でマルチスレッド処理する
    codec_ctx_->thread_type = FF_THREAD_SLICE;
    // ラズパイ4の「4コア」をフル活用して解凍速度を最大化
    codec_ctx_->thread_count = 4;

    // 3. 設定を適用して起動
    if (avcodec_open2(codec_ctx_, codec, nullptr) < 0) {
        throw std::runtime_error("エラー: デコーダの起動に失敗しました");
    }

    // データを入れる箱を準備
    frame_ = av_frame_alloc();
    pkt_ = av_packet_alloc();
}

H264Decoder::~H264Decoder() {
    // メモリを綺麗に掃除する
    if (frame_) av_frame_free(&frame_);
    if (pkt_) av_packet_free(&pkt_);
    if (codec_ctx_) avcodec_free_context(&codec_ctx_);
}

bool H264Decoder::decode_packet(const std::vector<uint8_t>& h264_data, YuvFrame& out_frame) {
    if (h264_data.empty()) return false;

    // 圧縮されたデータをFFmpegのカプセルにセット
    pkt_->data = const_cast<uint8_t*>(h264_data.data());
    pkt_->size = h264_data.size();

    // 1. カプセルを解凍機に投げ込む
    int ret = avcodec_send_packet(codec_ctx_, pkt_);
    if (ret < 0) return false;

    // 2. 解凍機から「生の映像（フレーム）」を取り出す
    // ※ 圧縮データの中身によっては、まだ1枚の絵になっていない（0が返ってこない）ことがある
    if (avcodec_receive_frame(codec_ctx_, frame_) == 0) {
        
        // C++の安全な構造体（YuvFrame）にポインタを繋ぎ替えて、外のプログラムに渡す
        out_frame.y_data = frame_->data[0];
        out_frame.y_pitch = frame_->linesize[0];
        out_frame.u_data = frame_->data[1];
        out_frame.u_pitch = frame_->linesize[1];
        out_frame.v_data = frame_->data[2];
        out_frame.v_pitch = frame_->linesize[2];
        out_frame.width = frame_->width;
        out_frame.height = frame_->height;
        
        return true; // 解凍成功！1枚の絵が完成！
    }

    return false; // まだ絵が完成していない
}