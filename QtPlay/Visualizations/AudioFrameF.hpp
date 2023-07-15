#pragma once

#include <span>
#include <vector>

#include "../Common/QtPlayCommon.hpp"

extern "C" {
#include "libavutil/frame.h"
#include "libavutil/samplefmt.h"
}

class AudioFrameF final {
 private:
  std::vector<float> m_data;
  int m_channels = 0, m_rate = 0;

 public:
  AudioFrameF(std::span<float> data, int chn, int rate)
      : m_data(data.begin(), data.end()), m_channels(chn), m_rate(rate){};
  AudioFrameF(const AVFrame& fr) {
    if (fr.format != AV_SAMPLE_FMT_FLTP) throw;
    const auto chn = fr.ch_layout.nb_channels;
    const auto rate = fr.sample_rate;
    const auto samples = fr.nb_samples;
    m_data.resize(chn * samples);
    auto data_csor = m_data.begin();
    for (int plane = 0; plane < chn; ++plane) {
      auto fr_data = qtplay::ptr_cast<const float>(fr.extended_data[plane]);
      std::copy(fr_data, fr_data + samples, data_csor);
      data_csor += samples;
    }
    m_channels = chn;
    m_rate = rate;
  }
  ~AudioFrameF() {}

  int channels() const { return m_channels; }
  int samples_per_chn() const {
    return m_data.size() / std::max(channels(), 1);
  }
  int rate() const { return m_rate; }
  double duration_s() const { return (double)samples_per_chn() / rate(); }

  std::span<float> getChannel(int chn) {
    if (chn < 0 || chn > channels()) throw;
    auto chn_begin = m_data.begin() + chn * samples_per_chn();
    return std::span(chn_begin, chn_begin + samples_per_chn());
  }
};
