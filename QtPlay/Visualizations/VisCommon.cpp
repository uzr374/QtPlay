#include "VisCommon.hpp"

#include <QCoreApplication>
#include <QThread>
#include <algorithm>

#include "../Common/QtPlayCommon.hpp"

using qtplay::logMsg;

auto clip_flt(float f) {
  return std::isnan(f) ? 0.0f : std::clamp(f, -1.0f, 1.0f);
}

VisCommon::VisCommon(QWidget* p, VisType type)
    : QWidget(p),
      vis_img(vis_img_size, QImage::Format::Format_RGB32),
      vis_type(type),
      update_timer(this) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  vis_img.fill(Qt::black);
  update_timer.setInterval(update_timeout);
  connect(&update_timer, &QTimer::timeout, [&] { update(); });
  connect(this, &VisCommon::setTimerStatus, this, &VisCommon::setTimStat,
          Qt::QueuedConnection);
}

VisCommon::~VisCommon() {
  if (rdft) av_rdft_end(rdft);
  if (rdft_data) av_free(rdft_data);
}

void VisCommon::requestUpdate() {
  if (thread() != QThread::currentThread()) {
    QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest),
                                Qt::LowEventPriority);
  } else {
    update();
  }
}

void VisCommon::setTimStat(bool s) {
  if (s)
    update_timer.stop();
  else
    update_timer.start();
}

void VisCommon::start() {
  std::scoped_lock lck(vis_mtx);
  if (m_paused) {
    m_paused = false;
    emit setTimerStatus(false);
  }
}

void VisCommon::stop() {
  std::scoped_lock lck(vis_mtx);
  if (!m_paused) {
    m_paused = true;
    emit setTimerStatus(true);
  }
}

void VisCommon::ensureBufferSize(int rate, int chn) {
  m_data.resize(std::ptrdiff_t(rate) * chn * 3,
                0.0f);  // To keep exactly 3 seconds of audio in the buffer to
                        // account for cases of pathologically high latency
}

void VisCommon::bufferAudio(std::span<const float> data, double latency,
                            int rate, int chn) {
  const auto write_time = qtplay::gettime();
  std::scoped_lock lck(vis_mtx);

  if (m_vis_channels != chn || m_vis_freq != rate) {
    m_data_write_idx = 0;
    m_vis_channels = chn;
    m_vis_freq = rate;
  }

  m_data_latency = latency;
  m_data_write_time = write_time;

  ensureBufferSize(rate, chn);

  while (!data.empty()) {
    const auto space_avaliable = m_data.size() - m_data_write_idx;
    const auto to_write = std::min(data.size(), space_avaliable);
    std::copy(data.begin(), data.begin() + to_write,
              m_data.begin() + m_data_write_idx);
    m_data_write_idx += to_write;
    if (m_data_write_idx == m_data.size()) m_data_write_idx = 0;
    data = data.subspan(to_write);
  }
}

void VisCommon::loadData(std::ptrdiff_t frames_to_load) {
  std::scoped_lock lck(vis_mtx);
  const auto freq = m_vis_freq, chn = m_vis_channels;
  tmp_freq = freq;
  tmp_channels = chn;
  if (freq <= 0 || chn <= 0) return;
  /* Get read index */
  auto get_start_idx = [](std::ptrdiff_t cur_idx,
                          std::ptrdiff_t samples_to_skip,
                          std::ptrdiff_t samples_count) -> std::ptrdiff_t {
    if (samples_to_skip > 0) {
      if (cur_idx >= samples_to_skip) {
        cur_idx -= samples_to_skip;  // Just move current index back
      } else {
        samples_to_skip -= cur_idx;  // Skip all elements behind the index
        cur_idx = 0;
        if (samples_to_skip > 0) {
          cur_idx = samples_count %
                    samples_to_skip;          // Loop the index over the buffer
          cur_idx = samples_count - cur_idx;  // And mirror it
        }
      }
    }

    return cur_idx;
  };
  // Calculate how much data is needed
  const auto samples_required =
      (std::max(frames_to_load, std::ptrdiff_t(vis_data_chunk * freq))) * chn;
  m_tmpdata.resize(samples_required, 0.0f);
  if (m_data.size() < samples_required) return;
  const auto time_since_write = std::max(
                 std::min(qtplay::gettime() - m_data_write_time, 0.05), 0.0),
             latency = m_data_latency + time_since_write;
  const auto samples_ahead = std::ptrdiff_t(latency * freq) * chn,
             start_read_idx =
                 get_start_idx(m_data_write_idx, samples_ahead, m_data.size()),
             to_read_before_ridx = std::min(start_read_idx, samples_required),
             to_read_after_ridx =
                 std::max(samples_required - to_read_before_ridx, 0ll);

  if (to_read_after_ridx > 0) {
    std::copy(m_data.end() - to_read_after_ridx, m_data.end(),
              m_tmpdata.begin());
  }

  if (to_read_before_ridx > 0) {
    std::copy(m_data.begin() + (start_read_idx - to_read_before_ridx),
              m_data.begin() + start_read_idx,
              m_tmpdata.begin() + to_read_after_ridx);
  }
}

void VisCommon::draw(QPainter& wp) {
  if (vis_type == VisType::WAVE) {
    constexpr auto rmsbar_w = 0.04;
    const auto vis_w = width(), vis_h = height();
    wp.fillRect(rect(), Qt::black);
    if (vis_w < 10) return;
    loadData();
    if (tmp_channels <= 0 || tmp_freq <= 0 || m_tmpdata.size() <= tmp_channels)
      return;
    const auto display_chn = tmp_channels;
    const auto height_per_chn =
        ((display_chn == 1) ? vis_h : ((vis_h / display_chn - 1) - 1));
    if (height_per_chn < 10) return;

    const auto samples_per_chn = m_tmpdata.size() / tmp_channels;
    const auto rmsbar_w_px = std::max(int((rmsbar_w)*vis_w), 1),
               bar_with_offset_w = std::max(int((rmsbar_w + 0.01) * vis_w), 1);
    for (auto chn = 0; chn < display_chn; ++chn) {
      const auto t = [&] {
        QTransform t;
        t.translate(bar_with_offset_w, chn * height_per_chn + chn);
        t.scale(qreal(vis_w - bar_with_offset_w) / (qreal(samples_per_chn)),
                qreal(height_per_chn) / 2.0);
        return t;
      }();

      double sum_of_squares = 0.0;
      QPainterPath wave_pth;
      wave_pth.reserve(samples_per_chn);

      auto map_sample = [&](std::ptrdiff_t col) {
        auto get_sample = [&](std::ptrdiff_t col) {
          return clip_flt(m_tmpdata[col * tmp_channels + chn]);
        };
        const auto clipped_sample = get_sample(col);
        const auto sample = clipped_sample + 1.0f;
        sum_of_squares += clipped_sample * clipped_sample;
        return t.map(QPointF(col, sample));
      };

      wave_pth.moveTo(map_sample(0));
      for (int col = 1; col < samples_per_chn; ++col) {
        wave_pth.lineTo(map_sample(col));
      }

      auto rms =
          std::clamp(std::sqrt(sum_of_squares / samples_per_chn), 0.0, 1.0);
      rms = 20.0 * std::log10(rms);
      rms = qMin<qreal>(qMax<qreal>(0.0, rms + 43.0) / 40.0, 1.0);

      wp.setPen(Qt::green);
      wp.drawPath(wave_pth);
      wp.fillRect(
          QRectF(0.0,
                 qreal((chn + 1) * height_per_chn + chn) - rms * height_per_chn,
                 rmsbar_w_px, rms * height_per_chn),
          Qt::green);

      if (chn < tmp_channels - 1) {
        const auto ly = (chn + 1) * height_per_chn + 1;
        wp.setPen(QColor(50, 90, 120));
        wp.drawLine(QPointF(0.0, ly), QPointF(vis_w, ly));
      }
    }
  } else {
    /*RDFT code adapted from FFplay*/
    const auto vis_h = vis_img_size.height(), vis_w = vis_img_size.width();
    const auto rdft_bits =
                   [vis_h] {
                     int rdft_bits = 1;
                     for (; (1 << rdft_bits) < 2 * vis_h; ++rdft_bits)
                       ;
                     return rdft_bits;
                   }(),
               nb_freq = 1 << (rdft_bits - 1);
    loadData(2 * nb_freq);
    if (tmp_channels <= 0 || tmp_freq <= 0 ||
        m_tmpdata.size() < 2 * nb_freq * tmp_channels) {
      wp.fillRect(rect(), Qt::black);
      vis_img.fill(Qt::black);
      xpos = 0;
      return;
    }
    if (xpos >= vis_w) {
      vis_img.fill(Qt::black);
      xpos = 0;
    }
    const auto nb_display_channels = std::min(tmp_channels, 2);
    if (rdft_bits != last_rdft_bits) {
      av_rdft_end(rdft);
      av_free(rdft_data);
      rdft = av_rdft_init(rdft_bits, DFT_R2C);
      last_rdft_bits = rdft_bits;
      rdft_data = qtplay::ptr_cast<FFTSample>(
          av_malloc_array(nb_freq, 4 * sizeof(*rdft_data)));
    }
    if (!rdft || !rdft_data) {
      logMsg("Failed to allocate RDFT buffers!");
      return;
    } else {
      FFTSample* data[2] = {};
      for (auto ch = 0; ch < nb_display_channels; ch++) {
        data[ch] = rdft_data + 2 * nb_freq * ch;
        auto i = ch;
        for (auto x = 0; x < 2 * nb_freq; x++) {
          double w = (x - nb_freq) * (1.0 / nb_freq);
          data[ch][x] = (m_tmpdata[i] * (INT16_MAX - 1)) * (1.0 - w * w);
          i += tmp_channels;
          if (i >= m_tmpdata.size()) i -= m_tmpdata.size();
        }

        av_rdft_calc(rdft, data[ch]);
      }
      const QRect rect{xpos, 0, 1, vis_h};
      const auto pitch = vis_img.bytesPerLine() / 4;
      auto pixels = (qtplay::ptr_cast<uint32_t>(vis_img.bits()) +
                     rect.width() * rect.y() + rect.x()) +
                    pitch * vis_h;
      for (auto y = 0; y < vis_h; y++) {
        const auto w = 1.0 / std::sqrt(nb_freq);
        const int a =
            std::min((sqrt(w * sqrt(data[0][2 * y + 0] * data[0][2 * y + 0] +
                                    data[0][2 * y + 1] * data[0][2 * y + 1]))),
                     255.0);
        const int b = std::min(
            ((nb_display_channels == 2)
                 ? sqrt(w * hypot(data[1][2 * y + 0], data[1][2 * y + 1]))
                 : a),
            255.0);
        pixels -= pitch;
        *pixels = std::uint32_t((a << 16) + (b << 8) + ((a + b) >> 1));
      }
      if (!m_paused) xpos++;
      wp.drawImage(this->rect(), vis_img);
    }
  }
}

void VisCommon::paintEvent(QPaintEvent* e) {
  QPainter p(this);
  draw(p);

  return QWidget::paintEvent(e);
}

void VisCommon::clearDisplay() {
  std::scoped_lock lck(vis_mtx);
  m_vis_channels = 0, m_vis_freq = 0;
  m_data_write_idx = 0;
  last_vis_time = 0.0, m_data_write_time = 0.0, m_data_latency = 0.0;
  requestUpdate();
}
