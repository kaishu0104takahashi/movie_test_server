#ifndef STREAM_APP_HPP_
#define STREAM_APP_HPP_

#include <string>
#include <atomic>
#include <memory>
#include "stream/receiver_thread.hpp"
#include "display/sdl_renderer.hpp"
#include "stream/control_relay.hpp"

class StreamApp {
public:
    StreamApp(int port, const std::string& title, int width, int height, DecodeMode mode);
    ~StreamApp();

    void run(std::atomic<bool>& keep_running);

private:
    std::unique_ptr<ReceiverThread> receiver_;
    std::unique_ptr<SdlRenderer> renderer_;
    std::unique_ptr<ControlRelay> relay_;
};

#endif