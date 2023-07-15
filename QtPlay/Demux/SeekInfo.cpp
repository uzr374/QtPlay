#include "SeekInfo.hpp"

void SeekInfo::reset() {
  seek_type = SEEK_NONE;
  incr_or_percent = 0.0;
  chapter_incr = 0;
  cycle_type = AVMEDIA_TYPE_UNKNOWN;
  st_idx_to_open = -1;
}

void SeekInfo::set_seek(SeekInfo::SeekType type, double val, int chapter_incr) {
  reset();
  this->chapter_incr = chapter_incr;
  seek_type = type;
  incr_or_percent = val;
}

void SeekInfo::set_stream_switch(AVMediaType type, int idx) {
  reset();
  cycle_type = type;
  st_idx_to_open = idx;
  seek_type = SEEK_STREAM_SWITCH;
}
