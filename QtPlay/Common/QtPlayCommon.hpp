#pragma once

#include <QAnyStringView>
#include <QScopeGuard>
#include <QtGlobal>
#include <bit>
#include <chrono>
#include <thread>

namespace qtplay {
using time_ms = std::chrono::milliseconds;
using steady_clock = std::chrono::steady_clock;
template <class F>
using scope_guard = ::QScopeGuard<F>;
#define ON_SCOPE_EXIT(f, name) qtplay::scope_guard<decltype(f)> name(f)

template <typename A, typename B>
inline constexpr auto ptr_cast(B* ptr) noexcept {
  return std::bit_cast<A*>(ptr);
}

inline void sleep_ms(int ms) { std::this_thread::sleep_for(time_ms(ms)); }
inline auto clk_now() { return steady_clock::now(); }
void logMsg(QAnyStringView fmt, ...);
double gettime() noexcept;
}  // namespace qtplay
