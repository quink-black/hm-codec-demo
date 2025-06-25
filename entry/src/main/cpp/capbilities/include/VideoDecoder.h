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

#ifndef VIDEODECODER_H
#define VIDEODECODER_H

#include "AVCodecSampleLog.h"
#include "SampleInfo.h"
#include "SampleCallback.h"

enum class DecoderBackend {
    HM = 0,
    FFmpeg = 1,
    FFmpeg_HW = 2,
};

class IVideoDecoder {
 public:
    static std::unique_ptr<IVideoDecoder> Create(DecoderBackend type);
    
    virtual ~IVideoDecoder() = default;

    virtual int32_t Create(const std::string &videoCodecMime) = 0;
    virtual int32_t Config(const SampleInfo &sampleInfo, CodecUserData *codecUserData) = 0;
    virtual int32_t PushInputBuffer(CodecBufferInfo &info) = 0;
    virtual int32_t FreeOutputBuffer(uint32_t bufferIndex, bool render) = 0;
    virtual int32_t Start() = 0;
    virtual int32_t Release() = 0;
};

#endif // VIDEODECODER_H