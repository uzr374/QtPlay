#include "Clock.hpp"

Clock::Clock() {}
Clock::~Clock() {}

double Clock::get_nolock() const {
  if (paused) {
    return pts;
  } else {
    const auto time_diff =
        std::min(std::max(qtplay::gettime() - last_updated, 0.0),
                 max_correction_tolerance);
    return pts + time_diff - time_diff * (1.0 - speed);
  }
}

double Clock::get() const {
  std::shared_lock lck(mtx);
  return get_nolock();
}

void Clock::set_nolock(double pts, double time) {
  this->pts = pts;
  this->last_updated = time;
}

void Clock::set_nolock_ex(double pts, double time, double max_tolerance,
                          bool paused) {
  this->max_correction_tolerance = max_tolerance;
  this->paused = paused;
  return set_nolock(pts, time);
}

void Clock::set(double pts, double time) {
  std::scoped_lock lck(mtx);
  return set_nolock(pts, time);
}

void Clock::set_speed(double spd) {
  std::scoped_lock lck(mtx);
  set_nolock(get_nolock());
  this->speed = spd;
}

void Clock::setPaused(bool p) {
  std::scoped_lock lck(mtx);
  if (p != paused) {
    set_nolock(get_nolock());
    paused = p;
  }
}
