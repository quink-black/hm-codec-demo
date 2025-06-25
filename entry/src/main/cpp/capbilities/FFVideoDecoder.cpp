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

#include "FFVideoDecoder.h"

#include "dfx/error/AVCodecSampleError.h"
#include <cerrno>

#undef LOG_TAG
#define LOG_TAG "FFVideoDecoder"

FFVideoDecoder::~FFVideoDecoder() {
    Release();
}

int32_t FFVideoDecoder::Create(const std::string &videoCodecMime) {
    const AVCodec *codec = nullptr;
    if (videoCodecMime == OH_AVCODEC_MIMETYPE_VIDEO_AVC)
        codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    else if (videoCodecMime == OH_AVCODEC_MIMETYPE_VIDEO_HEVC)
        codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
    else
        return AVCODEC_SAMPLE_ERR_ERROR;
        
    decoder_.reset(avcodec_alloc_context3(codec));
    AVCODEC_SAMPLE_LOGI("create decoder %{public}s", codec->name);;
    
    return AVCODEC_SAMPLE_ERR_OK;
}

int FFVideoDecoder::ReceiveFrame() {
    OH_AVBuffer *out_buffer = out_buffer_.get();
    AVFrame *frame = dec_frame_.get();
    
    while (true) {
        OH_AVCodecBufferAttr attr = {};
        
        int ret = avcodec_receive_frame(decoder_.get(), frame);
        if (ret < 0) {
            if (ret != AVERROR_EOF)
                return ret;
                
            attr.flags = AVCODEC_BUFFER_FLAGS_EOS;
            OH_AVBuffer_SetBufferAttr(out_buffer, &attr);
            SampleCallback::OnNewOutputBuffer(nullptr, 0, out_buffer, codecUserData_);
            return ret;
        }

        attr.pts = frame->pts;
        attr.size = 1;
        attr.offset = 0;
        attr.flags = 0;
        OH_AVBuffer_SetBufferAttr(out_buffer, &attr);
        SampleCallback::OnNewOutputBuffer(nullptr, 0, out_buffer, codecUserData_);

        {
            std::unique_lock<std::mutex> lk(frame_mutex_);
            frame_cond_.wait(lk, [this]() { return has_frame_ || quit_; });
            has_frame_ = false;
            if (quit_)
                return AVERROR_EOF;
        }

        // Now render frame
        RenderFrame();
    }
}

void FFVideoDecoder::ConvertFrame(AVFrame *src, AVFrame *dst, AVPixelFormat dst_format) {
    av_frame_unref(dst);

    if (src->format == dst_format) {
        av_frame_move_ref(dst, src);
        return;
    }

    SwsContext *sws_ctx = sws_getContext(src->width, src->height, (AVPixelFormat)src->format, src->width, src->height,
                                         dst_format, SWS_BILINEAR, NULL, NULL, NULL);

    dst->width = src->width;
    dst->height = src->height;
    dst->format = dst_format;
    av_frame_get_buffer(dst, 0);

    sws_scale(sws_ctx, (const uint8_t *const *)src->data, src->linesize, 0, src->height, dst->data, dst->linesize);
    sws_freeContext(sws_ctx);
}

int FFVideoDecoder::RenderFrame() {
    OHNativeWindowBuffer *buffer = nullptr;
    int fd = -1;
    OH_NativeBuffer_Format format = NATIVEBUFFER_PIXEL_FMT_BUTT;
    int32_t w = dec_frame_->width;
    int32_t h = dec_frame_->height;
    int32_t stride = 0;
    int32_t ret = OH_NativeWindow_NativeWindowHandleOpt(window_, GET_FORMAT, &format);
    ret = OH_NativeWindow_NativeWindowHandleOpt(window_, SET_BUFFER_GEOMETRY, w, h);
    
    AVPixelFormat pixel = AV_PIX_FMT_NONE;
    switch (format) {
    case NATIVEBUFFER_PIXEL_FMT_RGBX_8888:
        pixel = AV_PIX_FMT_RGB0;
        break;
    case NATIVEBUFFER_PIXEL_FMT_RGBA_8888:
        pixel = AV_PIX_FMT_RGBA;
        break;
    case NATIVEBUFFER_PIXEL_FMT_RGB_888:
        pixel = AV_PIX_FMT_RGB24;
        break;
    default:
        AVCODEC_SAMPLE_LOGE("Doesn't support window format %{public}d", format);;
        return AVERROR(ENOTSUP);
    }
    
    ConvertFrame(dec_frame_.get(), render_frame_.get(), pixel);
    
    OH_NativeWindow_NativeWindowRequestBuffer(window_, &buffer, &fd);
    
    OH_NativeBuffer *nb = nullptr;
    OH_NativeBuffer_FromNativeWindowBuffer(buffer, &nb);
    auto handle = OH_NativeWindow_GetBufferHandleFromNative(buffer);
    
    stride = handle->stride;
    
    void *p = nullptr;
    OH_NativeBuffer_Map(nb, &p);
    
    const AVFrame *frame = render_frame_.get();
    uint8_t *src = frame->data[0];
    uint8_t *dst = static_cast<uint8_t *>(p);
    for (int i = 0; i < h; i++) {
        memcpy(dst, src, std::min(frame->linesize[0], stride));
        src += frame->linesize[0];
        dst += stride;
    }
    OH_NativeBuffer_Unmap(nb);
    
    Region region = {};
    OH_NativeWindow_NativeWindowFlushBuffer(window_, buffer, fd, region);
    
    return 0;
}
    
void FFVideoDecoder::Thread() {
    uint8_t *in_addr = OH_AVBuffer_GetAddr(in_buffer_.get());
    
    while (true) {
        SampleCallback::OnNeedInputBuffer(nullptr, 0, in_buffer_.get(), codecUserData_);
        
        {
            std::unique_lock<std::mutex> lk(pkt_mutex_);
            pkt_cond_.wait(lk, [this](){
                return has_pkt_ || quit_;
            });
            if (quit_)
                break;
            has_pkt_ = false;
        }
        
        OH_AVCodecBufferAttr attr = {};
        OH_AVErrCode err = OH_AVBuffer_GetBufferAttr(in_buffer_.get(), &attr);
        if (err != AV_ERR_OK) {
            AVCODEC_SAMPLE_LOGE("get buffer attr failed, %{public}d", err);;
            break;
        }
        // AVCODEC_SAMPLE_LOGD("get pkt size %{public}d, pts %{public}" PRId64, attr.size, attr.pts);;
        
        if (attr.size > 0) {
            pkt_->data = in_addr + attr.offset;
            pkt_->size = attr.size;
            pkt_->pts = attr.pts;
        } else {
            pkt_->data = nullptr;
            pkt_->size = 0;
        }
        
        int ret = avcodec_send_packet(decoder_.get(), pkt_.get());
        if (ret < 0)
            break;
            
        ret = ReceiveFrame();
        if (quit_)
            break;
        if (ret != AVERROR(EAGAIN))
            break;
    }
}

int32_t FFVideoDecoder::Config(const SampleInfo &sampleInfo, CodecUserData *codecUserData) {
    codecUserData_ = codecUserData;
    window_ = sampleInfo.window;

    decoder_->width = sampleInfo.videoWidth;
    decoder_->height = sampleInfo.videoHeight;
    decoder_->framerate = AVRational{static_cast<int>(sampleInfo.frameRate), 1};
    decoder_->pkt_timebase = AVRational{1, 1000000};

    int ret = avcodec_open2(decoder_.get(), decoder_->codec, nullptr);
    if (ret < 0) {
        AVCODEC_SAMPLE_LOGE("Open decoder failed, %{public}s", av_err2str(ret));
        return AVCODEC_SAMPLE_ERR_ERROR;
    }

    return AVCODEC_SAMPLE_ERR_OK;
}

int32_t FFVideoDecoder::Start() {
    thread_ = std::thread(&FFVideoDecoder::Thread, this);
    return AVCODEC_SAMPLE_ERR_OK;
}

int32_t FFVideoDecoder::PushInputBuffer(CodecBufferInfo &info) {
    {
        std::unique_lock<std::mutex> lk(pkt_mutex_);
        has_pkt_ = true;
        pkt_cond_.notify_one();
    }
    return AVCODEC_SAMPLE_ERR_OK;
}

int32_t FFVideoDecoder::FreeOutputBuffer(uint32_t bufferIndex, bool render) {
    {
        std::unique_lock<std::mutex> lk(frame_mutex_);
        has_frame_ = true;
        frame_cond_.notify_one();
    }
    return AVCODEC_SAMPLE_ERR_OK;
}

int32_t FFVideoDecoder::Release() {
    if (thread_.joinable()) {
        {
            std::unique_lock<std::mutex> lk(pkt_mutex_);
            quit_ = true;
            pkt_cond_.notify_one();
        }
        {
            std::unique_lock<std::mutex> lk(frame_mutex_);
            quit_ = true;
            frame_cond_.notify_one();
        }
        thread_.join();
    }
    
    decoder_ = nullptr;
    in_buffer_ = nullptr;
    out_buffer_ = nullptr;
    pkt_ = nullptr;
    dec_frame_ = nullptr;
    render_frame_ = nullptr;
    
    return 0;
}