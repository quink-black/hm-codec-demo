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
#include "libavutil/opt.h"
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
        codec = avcodec_find_encoder_by_name("h264_ohcodec");
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

int FFVideoEncoder::OutputData(uint8_t *data, int size, int64_t pts, uint32_t flags) {
    if (size > 0) {
        uint8_t *out_addr = OH_AVBuffer_GetAddr(out_buffer_.get());
        memcpy(out_addr, data, size);
    }
    OH_AVCodecBufferAttr attr = {
        .pts = pts,
        .size = size,
        .offset = 0,
        .flags = flags,
    };

    OH_AVBuffer_SetBufferAttr(out_buffer_.get(), &attr);
    SampleCallback::OnNewOutputBuffer(nullptr, 0, out_buffer_.get(), codecUserData_);

    {
        std::unique_lock<std::mutex> lk(pkt_mutex_);
        pkt_cond_.wait(lk, [this](){
            return release_pkt_ || quit_ || eof_ == 1;
        });
        if (quit_) {
            AVCODEC_SAMPLE_LOGI("quit");
            return AVERROR_EOF;
        }
        if (eof_ == 1)
            eof_ = 2;
        release_pkt_ = false;
    }
    return 0;
}

void FFVideoEncoder::Thread() {
    int ret;

    while (true) {
        if (eof_) {
            avcodec_send_frame(encoder_.get(), nullptr);
        } else {
            frame_->width = encoder_->width;
            frame_->height = encoder_->height;
            frame_->format = AV_PIX_FMT_NV12;
            frame_->pts = 0;
            ret = av_frame_get_buffer(frame_.get(), 0);
            if (ret < 0)
                break;
            frame_->format = AV_PIX_FMT_OHCODEC;
            ret = avcodec_send_frame(encoder_.get(), frame_.get());
            AVCODEC_SAMPLE_LOGI("Send frame %{public}d\n", ret);
            av_frame_unref(frame_.get());
        }

        ret = avcodec_receive_packet(encoder_.get(), pkt_.get());
        AVCODEC_SAMPLE_LOGI("receive pkt %{public}d\n", ret);
        if (ret < 0) {
            if (ret == AVERROR(EAGAIN)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            if (ret == AVERROR_EOF) {
                OutputData(nullptr, 0, 0, AVCODEC_BUFFER_FLAGS_EOS);
                AVCODEC_SAMPLE_LOGI("notify eof\n");
            }
            break;
        }

        size_t size;
        uint8_t *extradata = av_packet_get_side_data(pkt_.get(), AV_PKT_DATA_NEW_EXTRADATA, &size);
        if (extradata) {
            ret = OutputData(extradata, size, pkt_->pts, AVCODEC_BUFFER_FLAGS_CODEC_DATA);
            if (ret < 0)
                break;
        }

        uint32_t flags = (pkt_->flags & AV_PKT_FLAG_KEY) ? AVCODEC_BUFFER_FLAGS_SYNC_FRAME : 0;
        ret = OutputData(pkt_->data, pkt_->size, pkt_->pts, flags);
        if (ret < 0)
            break;
        av_packet_unref(pkt_.get());
    }
}

int32_t FFVideoEncoder::Config(SampleInfo &sampleInfo, CodecUserData *codecUserData) {
    codecUserData_ = codecUserData;

    encoder_->width = sampleInfo.videoWidth;
    encoder_->height = sampleInfo.videoHeight;
    encoder_->framerate = AVRational{static_cast<int>(sampleInfo.frameRate), 1};
    encoder_->time_base = AVRational{1, 1000000};
    encoder_->pix_fmt = AV_PIX_FMT_OHCODEC;
    encoder_->bit_rate = sampleInfo.bitrate;
    encoder_->flags = AV_CODEC_FLAG_GLOBAL_HEADER;

    int ret = avcodec_open2(encoder_.get(), encoder_->codec, nullptr);
    if (ret < 0) {
        AVCODEC_SAMPLE_LOGE("Open decoder failed, %{public}s", av_err2str(ret));
        return AVCODEC_SAMPLE_ERR_ERROR;
    }
    auto device_ctx = reinterpret_cast<AVHWDeviceContext *>(encoder_->hw_device_ctx->data);
    auto dev = static_cast<AVOHCodecDeviceContext *>(device_ctx->hwctx);
    sampleInfo.window = static_cast<OHNativeWindow *>(dev->native_window);
    OH_NativeWindow_NativeObjectReference(sampleInfo.window);
    AVCODEC_SAMPLE_LOGI("Get native window success, %{public}p", sampleInfo.window);

    return AVCODEC_SAMPLE_ERR_OK;
}

int32_t FFVideoEncoder::Start() {
    thread_ = std::thread(&FFVideoEncoder::Thread, this);
    return AVCODEC_SAMPLE_ERR_OK;
}

int32_t FFVideoEncoder::FreeOutputBuffer(uint32_t bufferIndex) {
    std::unique_lock<std::mutex> lk(pkt_mutex_);
    release_pkt_ = true;
    pkt_cond_.notify_one();
    return AVCODEC_SAMPLE_ERR_OK;
}

int32_t FFVideoEncoder::NotifyEndOfStream() {
    std::unique_lock<std::mutex> lk(pkt_mutex_);
    eof_ = true;
    pkt_cond_.notify_one();
    return 0;
}

int32_t FFVideoEncoder::Stop() {
    AVCODEC_SAMPLE_LOGI("Stop");
    {
        std::unique_lock<std::mutex> lk(pkt_mutex_);
        quit_ = true;
        pkt_cond_.notify_one();
    }

    return 0;
}

int32_t FFVideoEncoder::Release() {
    AVCODEC_SAMPLE_LOGI("Release >>>");
    if (thread_.joinable()) {
        {
            AVCODEC_SAMPLE_LOGI("Release 1 >>>");
            std::unique_lock<std::mutex> lk(pkt_mutex_);
            quit_ = true;
            pkt_cond_.notify_one();
        }
        AVCODEC_SAMPLE_LOGI("Release 2 >>>");
        thread_.join();
        AVCODEC_SAMPLE_LOGI("Release 3 >>>");
    }
    AVCODEC_SAMPLE_LOGI("Release 4 >>>");
    return 0;
}