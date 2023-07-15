#pragma once

extern "C" {
#include <libavcodec/codec_desc.h>
#include <libavformat/avformat.h>
#include <libavutil/display.h>
}

#include <cmath>

class Stream final {
 private:
  AVCodecParameters* m_codecpar = nullptr;
  AVRational m_tb = {0, 1}, m_fr = {0, 1}, m_avg_fr = {0, 1}, m_sar = {0, 1};
  int64_t start_time = AV_NOPTS_VALUE, m_duration = AV_NOPTS_VALUE;
  bool is_attached_pic = false, decoder_set_start_pts = false;
  const AVCodecDescriptor* codec_desc = nullptr;
  const AVCodec* m_codec = nullptr;
  double m_rotationAngle = 0.0;
  int m_index = -1;

  static void copyParams(Stream& dst, const Stream& src);

 public:
  static double get_rotation(const int32_t* const dm);
  void reset();
  int index() const;
  bool isValid() const;
  Stream();
  ~Stream();
  Stream(AVFormatContext* ctx, int index);
  Stream& operator=(const Stream& rhs);
  Stream& operator=(Stream&& rhs);
  Stream(Stream&& rhs);
  Stream(const Stream& rhs);

  AVRational tbR() const;
  double tb() const;
  AVRational sarR() const;
  double sar() const;
  AVRational avgFrameRateR() const;
  double avgFrameRate() const;
  AVRational frameRateR() const;
  double frameRate() const;
  bool isAttachedPic() const;
  bool setStartPts() const;
  const AVCodecParameters* codecpar() const;
  int64_t startTimeInt() const;
  int64_t durationInt() const;
  double duration() const;
  double startTime() const;
  AVCodecID codecID() const;
  const AVCodec* codec() const;
  bool isBitmapSub() const;
  double rotation() const;
};
