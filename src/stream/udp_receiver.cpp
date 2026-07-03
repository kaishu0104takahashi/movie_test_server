#include "stream/udp_receiver.hpp"
#include <stdexcept>

// 指定されたポートでUDP受信機を初期化するため
UdpReceiver::UdpReceiver(int port) {
    // FFmpegのネットワーク通信機能を使用可能にするため
    avformat_network_init();

    // 待ち受け用のURLと、パケットロスを防ぐための十分なバッファサイズを設定するため
    std::string url = "udp://0.0.0.0:" + std::to_string(port) + "?buffer_size=1024000";

    // 映像の遅延（レイテンシ）を極限まで削るためのオプション群を設定するため
    AVDictionary* options = nullptr;
    av_dict_set(&options, "fflags", "nobuffer", 0); // 内部バッファリングを無効化するため
    av_dict_set(&options, "flags", "low_delay", 0); // 低遅延モードを強制するため
    av_dict_set(&options, "max_delay", "0", 0);     // パケットの到着待ち時間をゼロにするため

    // ストリーム情報を保持するコンテキスト（箱）を確保するため
    fmt_ctx_ = avformat_alloc_context();
    
    // 設定したURLとオプションを用いて、ネットワークからのデータ受信口を開くため
    if (avformat_open_input(&fmt_ctx_, url.c_str(), nullptr, &options) < 0) {
        av_dict_free(&options);
        throw std::runtime_error("エラー: UDPポートを開けません");
    }
    
    // オプション適用後は不要になる辞書メモリを解放するため
    av_dict_free(&options);

    // 届いたデータの中にどのようなストリーム（映像、音声など）が含まれているか解析するため
    if (avformat_find_stream_info(fmt_ctx_, nullptr) < 0) {
        throw std::runtime_error("エラー: ストリーム情報が見つかりません");
    }

    // 複数あるストリームの中から、映像（ビデオ）のストリーム番号を探し出すため
    for (unsigned int i = 0; i < fmt_ctx_->nb_streams; i++) {
        if (fmt_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index_ = i;
            break;
        }
    }

    // 映像ストリームが1つも見つからなかった場合に安全にエラー終了させるため
    if (video_stream_index_ == -1) {
        throw std::runtime_error("エラー: ビデオストリームが見つかりません");
    }
}

// クラス破棄時にネットワークリソースを安全に解放するため
UdpReceiver::~UdpReceiver() {
    // 開いている入力ストリームがあれば閉じるため
    if (fmt_ctx_) avformat_close_input(&fmt_ctx_);
    
    // ネットワーク機能の後片付けを行うため
    avformat_network_deinit();
}

// ネットワークから届いたパケットを、メモリコピーなしで直接取得するため
bool UdpReceiver::receive_packet(AVPacket* pkt) {
    // UDPポートから1フレーム分のデータを読み込み、直接パケットのポインタに格納するため
    int ret = av_read_frame(fmt_ctx_, pkt);
    
    // 読み込みに失敗、またはデータがまだ来ていない場合は false を返すため
    if (ret < 0) return false;

    // 届いたパケットが映像ストリーム以外（