#include "Subtitle.hpp"

Subtitle::Subtitle() {}

Subtitle::~Subtitle() { clear(); }

Subtitle& Subtitle::operator=(Subtitle&& rhs) noexcept {
  clear();
  sub = rhs.sub;
  rhs.sub = {};
  m_width = rhs.m_width;
  m_height = rhs.m_height;
  pts = rhs.pts;
  duration = rhs.duration;
  start_display_time = rhs.start_display_time;
  end_display_time = rhs.end_display_time;
  m_frame = std::move(rhs.m_frame);

  return *this;
}

Subtitle::Subtitle(Subtitle&& rhs) noexcept { *this = std::move(rhs); }

void Subtitle::clear() {
  m_frame.clear();
  avsubtitle_free(&sub);
  m_width = m_height = 0;
  pts = duration = start_display_time = end_display_time = 0.0;
  uploaded = false;
}

bool Subtitle::isBitmap() const {
	return m_width > 0 && m_height > 0 && sub.format == 0;
}
