#include "AudioParams.hpp"

#include <utility>

AudioParams::AudioParams() {}

AudioParams::~AudioParams() { reset(); }

AudioParams::AudioParams(const AudioParams& p) { *this = p; }

AudioParams::AudioParams(AudioParams&& p) noexcept { *this = std::move(p); }

void AudioParams::reset() {
  freq = 0;
  ch_layout.reset();
  ch_layout = {};
  fmt = AV_SAMPLE_FMT_NONE;
}

AudioParams& AudioParams::operator=(const AudioParams& rhs) {
  reset();  // To reset ch_layout
  freq = rhs.freq;
  fmt = rhs.fmt;
  ch_layout = rhs.ch_layout;

  return *this;
}

AudioParams& AudioParams::operator=(AudioParams&& rhs) noexcept {
  reset();  // To reset ch_layout
  freq = rhs.freq;
  fmt = rhs.fmt;
  ch_layout = std::move(rhs.ch_layout);

  return *this;
}
