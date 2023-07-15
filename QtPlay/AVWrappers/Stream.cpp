#include "Stream.hpp"

void Stream::copyParams(Stream& dst, const Stream& src) {
  dst.m_tb = src.m_tb;
  dst.m_fr = src.m_fr;
  dst.m_avg_fr = src.m_avg_fr;
  dst.m_sar = src.m_sar;
  dst.start_time = src.start_time;
  dst.m_duration = src.m_duration;
  dst.is_attached_pic = src.is_attached_pic;
  dst.decoder_set_start_pts = src.decoder_set_start_pts;
  dst.m_codec = avcodec_find_decoder(src.codecID());
  dst.codec_desc = avcodec_descriptor_get(src.codecID());
  dst.m_rotationAngle = src.rotation();
  dst.m_index = src.index();
}

double Stream::get_rotation(const int32_t* const displaymatrix) {
  // TODO: use qtplay::ptr_cast instead of reinterpret_cast
  double theta = 0.0;
  if (displaymatrix)
    theta = -std::round(av_display_rotation_get(
        reinterpret_cast<const int32_t*>(displaymatrix)));

  theta -= 360.0 * std::floor(theta / 360.0 + 0.9 / 360.0);

  if (std::fabs(theta - 90.0 * std::round(theta / 90.0)) > 2.0)
    av_log(
        NULL, AV_LOG_WARNING,
        "Odd rotation angle.\n"
        "If you want to help, upload a sample "
        "of this file to https://streams.videolan.org/upload/ "
        "and contact the ffmpeg-devel mailing list. (ffmpeg-devel@ffmpeg.org)");

  return std::isnan(theta) ? 0.0 : theta;
}

void Stream::reset() {
  if (m_codecpar) avcodec_parameters_free(&m_codecpar);
  m_codecpar = nullptr;
  m_tb = {0, 1}, m_fr = {0, 1}, m_avg_fr = {0, 1}, m_sar = {0, 1};
  start_time = AV_NOPTS_VALUE;
  m_duration = AV_NOPTS_VALUE;
  is_attached_pic = false;
  decoder_set_start_pts = false;
  m_rotationAngle = 0.0;
  m_index = -1;
}

int Stream::index() const { return m_index; }

bool Stream::isValid() const { return m_codecpar && m_codec; }

Stream::Stream() : m_codecpar(avcodec_parameters_alloc()) {
  if (!m_codecpar) throw AVERROR(ENOMEM);
}

Stream::~Stream() { reset(); }

Stream::Stream(AVFormatContext* ctx, int index) : Stream() {
  if (!ctx || index < 0 || index >= ctx->nb_streams) return;

  auto st = ctx->streams[index];
  const auto codecpar = st->codecpar;

  if (!st || !codecpar) {
    return;
  }

  m_tb = st->time_base;
  start_time = st->start_time;
  m_duration = st->duration;
  m_index = index;
  codec_desc = avcodec_descriptor_get(codecpar->codec_id);
  m_codec = avcodec_find_decoder(codecpar->codec_id);
  if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
    is_attached_pic = (st->disposition & AV_DISPOSITION_ATTACHED_PIC);
    if (!is_attached_pic) {
      m_avg_fr = st->avg_frame_rate;
      m_fr = av_guess_frame_rate(ctx, st, nullptr);
      m_sar = av_guess_sample_aspect_ratio(ctx, st, nullptr);
      m_rotationAngle = get_rotation(static_cast<const int32_t* const>(
          static_cast<void*>(av_stream_get_side_data(
              st, AV_PKT_DATA_DISPLAYMATRIX, nullptr))));
    }
  } else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
    if (ctx->iformat->flags & AVFMT_NOTIMESTAMPS) {
      decoder_set_start_pts = true;
    }
  }

  if (!m_codecpar) {
    m_codecpar = avcodec_parameters_alloc();
    if (!m_codecpar) throw AVERROR(ENOMEM);
  }

  if (avcodec_parameters_copy(m_codecpar, codecpar) < 0) throw AVERROR(EINVAL);
}

Stream& Stream::operator=(const Stream& rhs) {
  if (!rhs.m_codecpar) {
    throw;
  }

  reset();
  copyParams(*this, rhs);

  if (!m_codecpar) {
    m_codecpar = avcodec_parameters_alloc();
    if (!m_codecpar) throw AVERROR(ENOMEM);
  }

  if (avcodec_parameters_copy(m_codecpar, rhs.m_codecpar) < 0)
    throw AVERROR(EINVAL);

  return *this;
}

Stream& Stream::operator=(Stream&& rhs) {
  *this = static_cast<const Stream&>(rhs);
  return *this;
}

Stream::Stream(Stream&& rhs) : Stream() { *this = rhs; }

Stream::Stream(const Stream& rhs) : Stream() { *this = rhs; }

AVRational Stream::tbR() const { return m_tb; }

double Stream::tb() const { return av_q2d(tbR()); }

AVRational Stream::sarR() const { return m_sar; }

double Stream::sar() const { return av_q2d(sarR()); }

AVRational Stream::avgFrameRateR() const { return m_avg_fr; }

double Stream::avgFrameRate() const { return av_q2d(avgFrameRateR()); }

AVRational Stream::frameRateR() const { return m_fr; }

double Stream::frameRate() const { return av_q2d(frameRateR()); }

bool Stream::isAttachedPic() const { return is_attached_pic; }

bool Stream::setStartPts() const { return decoder_set_start_pts; }

const AVCodecParameters* Stream::codecpar() const { return m_codecpar; }

int64_t Stream::startTimeInt() const { return start_time; }

int64_t Stream::durationInt() const { return m_duration; }

double Stream::duration() const { return tb() * durationInt(); }

double Stream::startTime() const { return tb() * startTimeInt(); }

AVCodecID Stream::codecID() const {
  return codecpar() ? codecpar()->codec_id : AV_CODEC_ID_NONE;
}

const AVCodec* Stream::codec() const { return m_codec; }

bool Stream::isBitmapSub() const {
  return codecpar() && codec_desc &&
         (codec_desc->props & AV_CODEC_PROP_BITMAP_SUB);
}

double Stream::rotation() const { return m_rotationAngle; }
