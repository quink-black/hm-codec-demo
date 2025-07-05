#pragma once

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

#include "VideoEncoder.h"

extern "C" {
#include "libavutil/error.h"
#include "libavutil/frame.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
}

class FFVideoEncoder final : public IVideoEncoder {
public:
    FFVideoEncoder(bool hwdev) : hwdev_(hwdev) {}
    ~FFVideoEncoder() override;

    int32_t Create(const std::string &videoCodecMime) override;
    int32_t Config(SampleInfo &sampleInfo, CodecUserData *codecUserData) override;
    int32_t Start() override;
    int32_t FreeOutputBuffer(uint32_t bufferIndex) override;
    int32_t NotifyEndOfStream() override;
    int32_t Stop() override;
    int32_t Release() override;

private:
    void Thread();
    
    int OutputData(uint8_t *data, int size, int64_t pts, uint32_t flags);

    bool hwdev_ = false;
    std::unique_ptr<AVCodecContext, void (*)(AVCodecContext *)> encoder_{
        nullptr, [](AVCodecContext *p) { avcodec_free_context(&p); }};
    CodecUserData *codecUserData_ = nullptr;
    OHNativeWindow *window_ = nullptr;

    std::unique_ptr<AVPacket, void (*)(AVPacket *)> pkt_{av_packet_alloc(), [](AVPacket *p) { av_packet_free(&p); }};
    std::unique_ptr<AVFrame, void (*)(AVFrame *)> frame_{av_frame_alloc(), [](AVFrame *p) { av_frame_free(&p); }};

    const int32_t out_buf_cap_ = 4 * 1024 * 1024;;
    std::unique_ptr<OH_AVBuffer, decltype(&OH_AVBuffer_Destroy)> out_buffer_ = {OH_AVBuffer_Create(out_buf_cap_),
                                                                               OH_AVBuffer_Destroy};

    std::thread thread_;

    std::mutex pkt_mutex_;
    std::condition_variable pkt_cond_;
    bool release_pkt_ = false;
    int eof_ = 0;
    bool quit_ = false;
};