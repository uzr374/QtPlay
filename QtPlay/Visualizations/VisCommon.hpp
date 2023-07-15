#pragma once

#include "../Common/QtPlayCommon.hpp"

extern "C" {
#include <libavcodec/avfft.h>
#include <libavutil/mem.h>
}

#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QWidget>
#include <QScreen>

#include <mutex>
#include <span>

class VisCommon final : public QWidget {
  Q_OBJECT;
  VisCommon() = delete;
  Q_DISABLE_COPY_MOVE(VisCommon);

  static constexpr auto update_timeout =
      17ull;  // In milliseconds(sorry std::chrono, maybe next time)
  static constexpr auto vis_data_chunk = 0.02;
  const QSize vis_img_size = screen()->size();

 private:
  void ensureBufferSize(int rate, int chn);
  void loadData(std::ptrdiff_t frames_to_load = 0);
  void draw(QPainter& wp);

  Q_SIGNAL void setTimerStatus(bool s);
  Q_SLOT void setTimStat(bool s);

 public:
  enum class VisType { WAVE, RDFT };

 protected:
  QImage vis_img;
  QTimer update_timer;
  
  std::mutex vis_mtx;
  std::vector<float> m_data, m_tmpdata;
  int m_vis_channels = 0, m_vis_freq = 0, tmp_freq = 0, tmp_channels = 0;
  std::ptrdiff_t m_data_write_idx = 0;
  double last_vis_time = 0.0, m_data_write_time = 0.0, m_data_latency = 0.0;
  bool m_paused = true;
  VisType vis_type = VisType::WAVE;

  // RDFT
  int xpos = 0, last_rdft_bits = 0;
  RDFTContext* rdft = nullptr;
  FFTSample* rdft_data = nullptr;

  //Waves
  double m_rmslevels[2] = {};

 protected:
  void paintEvent(QPaintEvent*) override;

 public:
  VisCommon(QWidget* p, VisType type);
  virtual ~VisCommon();

  void start();
  void stop();
  void clearDisplay();
  void requestUpdate();
  void bufferAudio(std::span<const float> data, double latency, int rate,
                   int chn);
};
