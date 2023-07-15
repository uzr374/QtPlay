#pragma once

#include "../AVWrappers/Frame.hpp"
#include "../AVWrappers/Packet.hpp"
#include "../AVWrappers/Stream.hpp"
#include "../AVWrappers/Subtitle.hpp"
#include "../Widgets/LoggerWidget.hpp"
#include "AudioParams.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavutil/pixfmt.h>
}

#include <QtGlobal>
#include <deque>

struct Decoder {
  Q_DISABLE_COPY_MOVE(Decoder);

  Stream stream;
  Frame buf_frame, downloaded_frame, filtered;
  AVCodecContext* avctx = nullptr;
  bool eof_state = false, isHW = false, use_hwdevice = false,
       use_hwframes = false;
  AVBufferRef* cached_fctx = nullptr;
  AVPixelFormat hw_pix_fmt = AV_PIX_FMT_NONE, sw_pix_fmt = AV_PIX_FMT_NONE;
  int64_t start_pts = 0LL, next_pts = 0LL;
  AVRational start_pts_tb = {}, next_pts_tb = {};
  static constexpr int extra_hwframes = 1;

  // Filtering context
  int last_w = 0, last_h = 0;
  AVPixelFormat last_format = (AVPixelFormat)-2;
  AVFilterGraph* graph = nullptr;
  AVFilterContext *filt_out = nullptr, *filt_in = nullptr;

  Decoder();
  virtual ~Decoder();
  // Do all fintering job(including reconfiguring) here
  void filter_decoded_videoframe(AVFrame* frame,
                                 std::deque<Frame>& filtered_frames);
  void filter_decoded_audioframe(AVFrame* frame,
                                 std::deque<Frame>& filtered_frames,
                                 AudioParams& audio_filter_src,
                                 const AudioParams& audio_tgt);
  void destroy();
  bool init_swdec(const Stream& st);
  bool init_hwdec(const Stream& st);
  bool init(const Stream& st);
  void flush();
  int decode_audio_packet(const Packet& apkt, std::deque<Frame>& decoded_frames,
                          AudioParams& audio_filter_src,
                          const AudioParams& audio_tgt);
  int decode_video_packet(const Packet& vpkt,
                          std::deque<Frame>& decoded_frames);
  int decode_subs(Packet& pkt, std::deque<Subtitle>& dst_subs);
};
