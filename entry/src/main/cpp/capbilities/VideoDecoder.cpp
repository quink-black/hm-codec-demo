#include "HMVideoDecoder.h"
#include "FFVideoDecoder.h"

std::unique_ptr<IVideoDecoder> IVideoDecoder::Create(DecoderBackend type) {
    switch (type) {
    case DecoderBackend::HM:
        return std::make_unique<VideoDecoder>();
    case DecoderBackend::FFmpeg:
        return std::make_unique<FFVideoDecoder>(false);
    case DecoderBackend::FFmpeg_HW:
        return std::make_unique<FFVideoDecoder>(true);
    default:
        return nullptr;
    }
}