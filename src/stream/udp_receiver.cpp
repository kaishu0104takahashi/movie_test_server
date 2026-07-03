#include "stream/udp_receiver.hpp"
#include <stdexcept>

UdpReceiver::UdpReceiver(int port) {
    avformat_network_init();

    std::string url = "udp://0.0.0.0:" + std::to_string(port) + "?buffer_size=1024000";

    AVDictionary* options = nullptr;
    av_dict_set(&options, "fflags", "nobuffer", 0);
    av_dict_set(&options, "flags", "low_delay", 0);
    av_dict_set(&options, "max_delay", "0", 0);

    fmt_ctx_ = avformat_alloc_context();
    if (avformat_open_input(&fmt_ctx_, url.c_str(), nullptr, &options) < 0) {
        throw std::runtime_error("エラー: UDPポートを開けません");
    }
    av_dict_free(&options);

    if (avformat_find_stream_info(fmt_ctx_, nullptr) < 0) {
        throw std::runtime_error("エラー: ストリーム情報が見つかりません");
    }

    for (unsigned int i = 0; i < fmt_ctx_->nb_streams; i++) {
        if (fmt_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index_ = i;
            break;
        }
    }

    if (video_stream_index_ == -1) {
        throw std::runtime_error("エラー: ビデオストリームが見つかりません");
    }
}

UdpReceiver::~UdpReceiver() {
    if (fmt_ctx_) avformat_close_input(&fmt_ctx_);
    avformat_network_deinit();
}

// ★ メモリコピーなし！ 届いたデータをそのままパケットのアドレスに入れる
bool UdpReceiver::receive_packet(AVPacket* pkt) {
    int ret = av_read_frame(fmt_ctx_, pkt);
    if (ret < 0) return false;

    if (pkt->stream_index != video_stream_index_) {
        av_packet_unref(pkt); // 映像以外なら捨てる
        return false;
    }
    return true;
}