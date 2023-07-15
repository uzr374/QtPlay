#pragma once

#include <QtGlobal>
#include <shared_mutex>

#include "QtPlayCommon.hpp"

struct Clock final {
  Q_DISABLE_COPY_MOVE(Clock);

  mutable std::shared_mutex mtx;
  double pts = NAN; /* clock base */
  double last_updated = NAN, speed = 1.0;
  double max_correction_tolerance = 2.0;
  bool paused = false;

  Clock();
  ~Clock();

  double get_nolock() const;
  double get() const;
  void set_nolock(double pts, double time = qtplay::gettime());
  void set_nolock_ex(double pts, double time, double max_tolerance,
                     bool paused);
  void set(double pts, double time = qtplay::gettime());
  void set_speed(double spd);
  void setPaused(bool p);
};
