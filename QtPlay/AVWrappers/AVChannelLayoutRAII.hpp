#pragma once

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
}

class AVChannelLayoutRAII final {
 private:
  AVChannelLayout ch_lout = {};

 public:
  AVChannelLayoutRAII();
  AVChannelLayoutRAII(const AVChannelLayout& lout);
  AVChannelLayoutRAII(const AVChannelLayoutRAII& lout);
  AVChannelLayoutRAII(AVChannelLayoutRAII&& lout) noexcept;
  ~AVChannelLayoutRAII();
  AVChannelLayoutRAII& operator=(const AVChannelLayout& lout);
  AVChannelLayoutRAII& operator=(const AVChannelLayoutRAII& lout);
  AVChannelLayoutRAII& operator=(AVChannelLayoutRAII&& lout) noexcept;
  void reset();
  const AVChannelLayout* readPtr() const;
  AVChannelLayout* rawPtr();
  int chCount() const;
};
