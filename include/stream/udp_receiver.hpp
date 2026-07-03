/**
 * @file udp_receiver.hpp
 * @brief UDPからパケットを受け取り、ポインタ（アドレス）で直接返す受信機
 */
#ifndef UDP_RECEIVER_HPP_
#define UDP_RECEIVER_HPP_

#include <string>

extern "C" {
#include <libavformat/avformat.h>
}

class UdpReceiver {
public:
    explicit UdpReceiver(int port);
    ~UdpReceiver();

    UdpReceiver(const UdpReceiver&) = delete;
    UdpReceiver& operator=(const UdpReceiver&) = delete;

    // ★ 変更：std::vectorではなく、FFmpegのパケットに直接データを受け取る
    bool receive_packet(AVPacket* pkt);

private:
    AVFormatContext* fmt_ctx_ = nullptr;
    int video_stream_index_ = -1;
};

#endif