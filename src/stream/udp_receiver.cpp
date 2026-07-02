#include "stream/udp_receiver.hpp"
#include <stdexcept>
#include <iostream>

UdpReceiver::UdpReceiver(int port) {
    // ネットワーク機能の初期化
    avformat_network_init();

    // 待ち受けURL（0.0.0.0 は「どのIPから飛んできても受け止める」という指定）
    //std::string url = "udp://0.0.0.0:" + std::to_string(port);
    std::string url = "udp://0.0.0.0:" + std::to_string(port) + "?buffer_size=1024000";

    // =======================================================
    // ★ 遠隔操作EV用：バッファリングを殺す「超低遅延マジック」
    // =======================================================
    AVDictionary* options = nullptr;
    av_dict_set(&options, "fflags", "nobuffer", 0); // データを一切溜め込まない
    av_dict_set(&options, "flags", "low_delay", 0); // 低遅延モード強制
    av_dict_set(&options, "max_delay", "0", 0);     // 待機時間ゼロ

    fmt_ctx_ = avformat_alloc_context();

    // 扉（ポート）を開けて、通信の受け入れを開始する
    if (avformat_open_input(&fmt_ctx_, url.c_str(), nullptr, &options) < 0) {
        throw std::runtime_error("エラー: UDPポート " + std::to_string(port) + " を開けませんでした");
    }
    av_dict_free(&options); // オプション設定用のメモリを解放

    // 飛んできたMPEG-TSの中から、ストリーム（映像や音声の通り道）の情報を探り出す
    if (avformat_find_stream_info(fmt_ctx_, nullptr) < 0) {
        throw std::runtime_error("エラー: ストリーム情報が見つかりません");
    }

    // 複数あるかもしれないレーンの中から「映像（VIDEO）」が流れているレーン番号を特定する
    for (unsigned int i = 0; i < fmt_ctx_->nb_streams; i++) {
        if (fmt_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index_ = i;
            break;
        }
    }

    if (video_stream_index_ == -1) {
        throw std::runtime_error("エラー: ビデオストリームが見つかりません");
    }

    // データを受け取るための空のカプセルを用意
    pkt_ = av_packet_alloc();
}

UdpReceiver::~UdpReceiver() {
    // 扉を閉めてメモリを掃除する
    if (pkt_) av_packet_free(&pkt_);
    if (fmt_ctx_) avformat_close_input(&fmt_ctx_);
    avformat_network_deinit();
}

bool UdpReceiver::receive_packet(std::vector<uint8_t>& out_h264_data) {
    // ネットワークからカプセル（パケット）を1つ取り出す
    // ※データが届いていない場合は、届くまでここで一瞬「待機（ブロック）」してくれます
    int ret = av_read_frame(fmt_ctx_, pkt_);
    if (ret < 0) {
        return false; // 通信切断やエラー
    }

    bool success = false;
    // 取り出したカプセルが「映像レーン」のものか確認する（音声などのゴミを弾く）
    if (pkt_->stream_index == video_stream_index_) {
        // カプセルの中身（H.264データ）を、C++の安全な配列にコピーして渡す
        out_h264_data.assign(pkt_->data, pkt_->data + pkt_->size);
        success = true;
    }

    // 次の受信に備えてカプセルを空っぽにする
    av_packet_unref(pkt_); 
    return success;
}