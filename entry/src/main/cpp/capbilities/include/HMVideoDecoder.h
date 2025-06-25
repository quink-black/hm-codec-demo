#pragma once

#include "multimedia/player_framework/native_avcodec_videodecoder.h"
#include "multimedia/player_framework/native_avbuffer_info.h"
#include "dfx/error/AVCodecSampleError.h"

#include "VideoDecoder.h"

class VideoDecoder final : public IVideoDecoder {
public:
    VideoDecoder() = default;
    ~VideoDecoder();

    int32_t Create(const std::string &videoCodecMime) override;
    int32_t Config(const SampleInfo &sampleInfo, CodecUserData *codecUserData) override;
    int32_t PushInputBuffer(CodecBufferInfo &info) override;
    int32_t FreeOutputBuffer(uint32_t bufferIndex, bool render) override;
    int32_t Start() override;
    int32_t Release() override;

private:
    int32_t SetCallback(CodecUserData *codecUserData);
    int32_t Configure(const SampleInfo &sampleInfo);

    bool isAVBufferMode_ = false;
    OH_AVCodec *decoder_;
};

