#pragma once

extern "C" {
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

inline constexpr AVPixelFormat supported_pix_fmts[] = {
    AV_PIX_FMT_YUV444P,  AV_PIX_FMT_YUVJ444P, AV_PIX_FMT_YUV420P,
    AV_PIX_FMT_YUVJ420P, AV_PIX_FMT_YUV422P,  AV_PIX_FMT_YUVJ422P,
    AV_PIX_FMT_YUV410P,  AV_PIX_FMT_YUV411P,  AV_PIX_FMT_YUVJ411P,
    AV_PIX_FMT_YUV440P,  AV_PIX_FMT_YUVJ440P, AV_PIX_FMT_NV12,
    AV_PIX_FMT_NV24,     AV_PIX_FMT_NV21,     AV_PIX_FMT_NV42,
    AV_PIX_FMT_NONE,
};

inline bool isSupportedOutput(AVPixelFormat pixFmt) {
  for (int i = 0; supported_pix_fmts[i] != AV_PIX_FMT_NONE; ++i) {
    if (supported_pix_fmts[i] == pixFmt && sws_isSupportedOutput(pixFmt) > 0 &&
        sws_isSupportedInput(pixFmt) > 0)
      return true;
  }
  return false;
}
