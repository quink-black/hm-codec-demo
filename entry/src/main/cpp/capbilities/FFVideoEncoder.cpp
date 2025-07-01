/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "FFVideoEncoder.h"

#include "dfx/error/AVCodecSampleError.h"
#include <cerrno>

extern "C" {
#include "libavutil/hwcontext.h"
#include "libavutil/hwcontext_oh.h"
}

#undef LOG_TAG
#define LOG_TAG "FFVideoEncoder"

FFVideoEncoder::~FFVideoEncoder() {
    Release();
}

static void LogCb(void *ctx, int level, const char *fmt, va_list va) {
    char buf[4096] = {0};

    vsnprintf(buf, sizeof(buf), fmt, va);
    if (level <= AV_LOG_ERROR)
        AVCODEC_SAMPLE_LOGE("%{public}s", buf);
    else if (level <= AV_LOG_WARNING)
        AVCODEC_SAMPLE_LOGW("%{public}s", buf);
    else if (level <= AV_LOG_INFO)
        AVCODEC_SAMPLE_LOGI("%{public}s", buf);
    else
        AVCODEC_SAMPLE_LOGD("%{public}s", buf);
}

int32_t FFVideoEncoder::Create(const std::string &videoCodecMime) {
    av_log_set_callback(&LogCb);

    const AVCodec *codec = nullptr;
    if (videoCodecMime == OH_AVCODEC_MIMETYPE_VIDEO_AVC)
        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    else if (videoCodecMime == OH_AVCODEC_MIMETYPE_VIDEO_HEVC)
        codec = avcodec_find_encoder_by_name("hevc_ohcodec");
    else
        return AVCODEC_SAMPLE_ERR_ERROR;

    if (codec == nullptr)
        return AVCODEC_SAMPLE_ERR_ERROR;
        
    encoder_.reset(avcodec_alloc_context3(codec));
    AVCODEC_SAMPLE_LOGI("create decoder %{public}s", codec->name);;
    
    return AVCODEC_SAMPLE_ERR_OK;
}

void FFVideoEncoder::Thread() {
}

int32_t FFVideoEncoder::Config(SampleInfo &sampleInfo, CodecUserData *codecUserData) {
    codecUserData_ = codecUserData;
    window_ = sampleInfo.window;

    encoder_->width = sampleInfo.videoWidth;
    encoder_->height = sampleInfo.videoHeight;
    encoder_->framerate = AVRational{static_cast<int>(sampleInfo.frameRate), 1};
    encoder_->pkt_timebase = AVRational{1, 1000000};
    encoder_->pix_fmt = AV_PIX_FMT_OHCODEC;

    if (hwdev_) {
        AVBufferRef *hw = nullptr;
        int ret = av_hwdevice_ctx_create(&hw, AV_HWDEVICE_TYPE_OHCODEC, nullptr, nullptr, 0);
        if (ret < 0) {
            AVCODEC_SAMPLE_LOGE("create hwdevice failed, %{public}d, %{public}s", ret, av_err2str(ret));
            return AVCODEC_SAMPLE_ERR_ERROR;
        }

        auto device_ctx = reinterpret_cast<AVHWDeviceContext *>(hw->data);
        auto dev = static_cast<AVOHCodecDeviceContext *>(device_ctx->hwctx);
        dev->native_window = sampleInfo.window;
        encoder_->hw_device_ctx = hw;
    }

    int ret = avcodec_open2(encoder_.get(), encoder_->codec, nullptr);
    if (ret < 0) {
        AVCODEC_SAMPLE_LOGE("Open decoder failed, %{public}s", av_err2str(ret));
        return AVCODEC_SAMPLE_ERR_ERROR;
    }

    return AVCODEC_SAMPLE_ERR_OK;
}

int32_t FFVideoEncoder::Start() {
    thread_ = std::thread(&FFVideoEncoder::Thread, this);
    return AVCODEC_SAMPLE_ERR_OK;
}

int32_t FFVideoEncoder::FreeOutputBuffer(uint32_t bufferIndex) {
    return AVCODEC_SAMPLE_ERR_OK;
}

int32_t FFVideoEncoder::NotifyEndOfStream() {
    return 0;
}

int32_t FFVideoEncoder::Stop() {
    return 0;
}

int32_t FFVideoEncoder::Release() {
    return 0;
}