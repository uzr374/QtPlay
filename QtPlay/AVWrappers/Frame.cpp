#include "Frame.hpp"

#include <utility>

void Frame::copyParams(const Frame& from, Frame& to) {
  to.duration = from.duration;
  to.pts = from.pts;
  to.pix_desc = av_pix_fmt_desc_get(from.pixFmt());
  to.m_type = from.m_type;
  to.uploaded = false;
}

void Frame::resetParams(Frame& fr) {
  fr.pts = NAN;
  fr.duration = NAN;
  fr.uploaded = false;
  fr.pix_desc = nullptr;
  fr.m_type = FRAME_UNKNOWN;
}

Frame::Frame() : m_frame(av_frame_alloc()) {
  if (!m_frame) throw AVERROR(ENOMEM);
}

Frame::Frame(AVFrame* src) : Frame() { av_frame_move_ref(m_frame, src); }

Frame::~Frame() {
  clear();
  av_frame_free(&m_frame);
}

Frame::Frame(const Frame& fr) : Frame() {
  Frame::copyParams(fr, *this);
  av_frame_ref(m_frame, fr.m_frame);
}

Frame::Frame(Frame&& fr) : Frame() {
  Frame::copyParams(fr, *this);
  av_frame_move_ref(m_frame, fr.m_frame);
}

Frame& Frame::operator=(const Frame& fr) {
  av_frame_unref(m_frame);
  Frame::copyParams(fr, *this);
  av_frame_ref(m_frame, fr.m_frame);

  return *this;
}

Frame& Frame::operator=(Frame&& fr) {
  av_frame_unref(m_frame);
  Frame::copyParams(fr, *this);
  av_frame_move_ref(m_frame, fr.m_frame);

  return *this;
}

void Frame::clear() {
  av_frame_unref(m_frame);
  resetParams(*this);
}

void Frame::setTs(double ts) { pts = ts; }

void Frame::calcTs(double tb) {
  pts = (m_frame->pts == AV_NOPTS_VALUE) ? NAN : tb * m_frame->pts;
}

bool Frame::ptsValid() const { return m_frame->pts != AV_NOPTS_VALUE; }

int64_t Frame::ptsInt() const { return m_frame->pts; }

void Frame::setPtsInt(int64_t ts) { m_frame->pts = ts; }

int Frame::sampleRate() const { return m_frame->sample_rate; }

int Frame::nbSamples() const { return m_frame->nb_samples; }

AVChannelLayout* Frame::channelLayout() const { return &m_frame->ch_layout; }

int Frame::channelCount() const { return m_frame->ch_layout.nb_channels; }

bool Frame::isValid() const {
  return m_frame->buf[0] ||
         (m_frame->width > 0 && m_frame->height > 0 && m_frame->data[0]);
}

int Frame::chromaShiftW() const {
  return pix_desc ? pix_desc->log2_chroma_w : 0;
}

int Frame::chromaShiftH() const {
  return pix_desc ? pix_desc->log2_chroma_h : 0;
}

int Frame::width(int plane) const {
  if ((plane != 0) && pix_desc)
    return AV_CEIL_RSHIFT(m_frame->width, pix_desc->log2_chroma_w);
  return m_frame->width;
}

int Frame::height(int plane) const {
  if (plane != 0 && pix_desc)
    return AV_CEIL_RSHIFT(m_frame->height, pix_desc->log2_chroma_h);
  return m_frame->height;
}

int Frame::linesize(int i) const { return m_frame->linesize[i]; }

uint8_t** Frame::data() { return m_frame->data; }

uint8_t* Frame::data(int plane) { return m_frame->data[plane]; }

AVRational Frame::sar() const { return m_frame->sample_aspect_ratio; }

AVFrame* Frame::av() const { return m_frame; }

AVPixelFormat Frame::pixFmt() const { return (AVPixelFormat)m_frame->format; }

AVSampleFormat Frame::sampleFmt() const {
  return (AVSampleFormat)m_frame->format;
}

AVColorSpace Frame::colorspace() const {
  return m_frame->colorspace;
}

bool Frame::limited() const { return m_frame->color_range != AVCOL_RANGE_JPEG; }

int64_t Frame::bytePos() const { return m_frame->pkt_pos; }

int* Frame::linesizes() const { return m_frame->linesize; }

bool Frame::allocate(int w, int h, AVPixelFormat fmt, int align) {
  if (!isValid() || (width() != w || height() != h || pixFmt() != fmt)) {
    av_frame_unref(m_frame);

    m_frame->width = w;
    m_frame->height = h;
    m_frame->format = fmt;

    if (av_frame_get_buffer(m_frame, align) < 0) {
      clear();
      return false;
    }

    av_frame_make_writable(m_frame);

    pix_desc = av_pix_fmt_desc_get(pixFmt());
  }

  return true;
}

FrameType Frame::type() const { return m_type; }

void Frame::moveTo(Frame& dst) {
  av_frame_unref(dst.m_frame);
  Frame::copyParams(*this, dst);
  av_frame_move_ref(dst.m_frame, m_frame);
  resetParams(*this);
}
