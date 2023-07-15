#include "QtPlayCommon.hpp"

double qtplay::gettime() noexcept {
  using namespace std::chrono;
  return duration<double>(high_resolution_clock::now().time_since_epoch())
      .count();
}
