#pragma once

#include "../AVWrappers/AVChannelLayoutRAII.hpp"

extern "C" {
#include <libavutil/samplefmt.h>
}

struct AudioParams final {
  AVChannelLayoutRAII ch_layout;
  AVSampleFormat fmt = AV_SAMPLE_FMT_NONE;
  int freq = 0;

  AudioParams();
  ~AudioParams();
  AudioParams(const AudioParams& p);
  AudioParams(AudioParams&& p) noexcept;
  AudioParams& operator=(const AudioParams& rhs);
  AudioParams& operator=(AudioParams&& rhs) noexcept;
  void reset();
};
