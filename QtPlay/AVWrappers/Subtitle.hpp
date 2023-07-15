#pragma once

#include "Frame.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
}

class Subtitle final {
 private:
  Subtitle(const Subtitle&) = delete;
  Subtitle& operator=(const Subtitle&) = delete;

 public:
  AVSubtitle sub = {};
  int m_width = 0, m_height = 0;
  double pts = 0.0, duration = 0.0, start_display_time = 0.0,
         end_display_time = 0.0;
  bool uploaded = false;
  Frame m_frame;

  Subtitle();
  ~Subtitle();
  Subtitle& operator=(Subtitle&& rhs) noexcept;
  Subtitle(Subtitle&& rhs) noexcept;

  void clear();
  bool isBitmap() const;
};
