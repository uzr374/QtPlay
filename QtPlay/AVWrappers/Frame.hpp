#pragma once

#include <QtGlobal>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/pixdesc.h>
}

enum FrameType {
  FRAME_VIDEO = 0,
  FRAME_AUDIO,
  FRAME_EOF,
  FRAME_ATTACHMENT,
  FRAME_UNKNOWN,
  FRAMETYPE_COUNT
};

/* Common struct for handling all types of decoded data and allocated render
 * buffers. */
struct Frame final {
  AVFrame* m_frame = nullptr;
  double pts = NAN;      /* presentation timestamp for the frame */
  double duration = NAN; /* estimated duration of the frame */
  bool uploaded = false;
  const AVPixFmtDescriptor* pix_desc = nullptr;
  FrameType m_type = FRAME_UNKNOWN;

  static void copyParams(const Frame& from, Frame& to);
  static void resetParams(Frame& fr);

  Frame();
  ~Frame();

  /* Takes ownership of src and resets it */
  Frame(AVFrame* src);
  Frame(const Frame& fr);
  Frame(Frame&& fr);
  Frame& operator=(const Frame& fr);
  Frame& operator=(Frame&& fr);

  void clear();
  bool isValid() const;
  void setTs(double ts);
  void calcTs(double tb);
  int chromaShiftW() const;
  int chromaShiftH() const;
  int width(int plane = 0) const;
  int height(int plane = 0) const;
  int linesize(int i = 0) const;
  uint8_t** data();
  uint8_t* data(int plane);
  AVRational sar() const;
  AVFrame* av() const;
  AVPixelFormat pixFmt() const;
  AVSampleFormat sampleFmt() const;
  int sampleRate() const;
  int nbSamples() const;
  AVChannelLayout* channelLayout() const;
  int channelCount() const;
  AVColorSpace colorspace() const;
  bool limited() const;
  int64_t bytePos() const;
  int* linesizes() const;
  bool ptsValid() const;
  int64_t ptsInt() const;
  void setPtsInt(int64_t ts);
  bool allocate(int w, int h, AVPixelFormat fmt, int align = 0);
  void moveTo(Frame& dst);
  FrameType type() const;
};

template <typename Managed>
class ScopeManager final {
  Q_DISABLE_COPY_MOVE(ScopeManager);

 private:
  Managed& managed;

 public:
  ScopeManager() = delete;
  ScopeManager(Managed& fr) : managed(fr){};
  ~ScopeManager() { managed.clear(); };
};
