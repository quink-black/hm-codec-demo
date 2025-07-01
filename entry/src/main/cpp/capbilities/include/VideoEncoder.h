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

#ifndef VIDEOENCODER_H
#define VIDEOENCODER_H

#include <memory>

#include "SampleInfo.h"
#include "SampleCallback.h"

enum class EncoderBackend {
    HM = 0,
    FFmpeg = 1,
    FFmpeg_HW = 2,
};

class IVideoEncoder {
public:
    static std::unique_ptr<IVideoEncoder> Create(EncoderBackend type);

    virtual ~IVideoEncoder() = default;

    virtual int32_t Create(const std::string &videoCodecMime) = 0;
    virtual int32_t Config(SampleInfo &sampleInfo, CodecUserData *codecUserData) = 0;
    virtual int32_t Start() = 0;
    virtual int32_t FreeOutputBuffer(uint32_t bufferIndex) = 0;
    virtual int32_t NotifyEndOfStream() = 0;
    virtual int32_t Stop() = 0;
    virtual int32_t Release() = 0;
};
#endif // VIDEOENCODER_H