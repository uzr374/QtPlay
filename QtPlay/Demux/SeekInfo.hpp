#pragma once

extern "C" {
#include <libavutil/avutil.h>
}

struct SeekInfo final {
  enum SeekType {
    SEEK_INCR,
    SEEK_PERCENT,
    SEEK_CHAPTER,
    SEEK_STREAM_SWITCH,
    SEEK_NONE
  };

  SeekType seek_type = SEEK_NONE;
  double incr_or_percent = 0.0;
  int chapter_incr = 0;
  AVMediaType cycle_type = AVMEDIA_TYPE_UNKNOWN;
  int st_idx_to_open = -1;

  SeekInfo() = default;
  ~SeekInfo() = default;

  void reset();
  void set_seek(SeekInfo::SeekType type, double val, int chapter_incr = 0);
  void set_stream_switch(AVMediaType type, int idx);
};
