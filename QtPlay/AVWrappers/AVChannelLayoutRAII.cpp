#include "AVChannelLayoutRAII.hpp"

#include <utility>

void AVChannelLayoutRAII::reset() {
  av_channel_layout_uninit(&ch_lout);
  ch_lout = {};
}

AVChannelLayoutRAII& AVChannelLayoutRAII::operator=(
    const AVChannelLayout& lout) {
  reset();
  if (av_channel_layout_copy(&ch_lout, &lout) < 0) throw AVERROR(ENOMEM);

  return *this;
}

AVChannelLayoutRAII& AVChannelLayoutRAII::operator=(
    const AVChannelLayoutRAII& lout) {
  return (*this = lout.ch_lout);
}

AVChannelLayoutRAII& AVChannelLayoutRAII::operator=(
    AVChannelLayoutRAII&& lout) noexcept {
  reset();
  ch_lout = lout.ch_lout;
  lout.ch_lout = {};

  return *this;
}

AVChannelLayoutRAII::AVChannelLayoutRAII() {}

AVChannelLayoutRAII::AVChannelLayoutRAII(const AVChannelLayout& lout)
    : AVChannelLayoutRAII() {
  *this = lout;
}

AVChannelLayoutRAII::AVChannelLayoutRAII(const AVChannelLayoutRAII& lout)
    : AVChannelLayoutRAII() {
  *this = lout;
}

AVChannelLayoutRAII::AVChannelLayoutRAII(AVChannelLayoutRAII&& lout) noexcept
    : AVChannelLayoutRAII() {
  *this = std::move(lout);
}

AVChannelLayoutRAII::~AVChannelLayoutRAII() { reset(); }

const AVChannelLayout* AVChannelLayoutRAII::readPtr() const {
  return std::addressof(ch_lout);
}

AVChannelLayout* AVChannelLayoutRAII::rawPtr() {
  return std::addressof(ch_lout);
}

int AVChannelLayoutRAII::chCount() const { return readPtr()->nb_channels; }
