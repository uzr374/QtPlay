#include "CThread.hpp"

#include "../Widgets/LoggerWidget.hpp"
#include "QtPlayCommon.hpp"

using qtplay::logMsg;
using ScopedLocker = CThread::ScopedLocker;

ScopedLocker::ScopedLocker(std::unique_ptr<CThread>& _thr) : managed(_thr) {
  if (!managed) {
    logMsg("CThread::ScopedLocker info: no thread provided");
    return;
  }

  if (!managed->isRunning()) {
    logMsg("CThread::ScopedLocker error: the thread is not running");
    return;
  }

  holds_lock = was_paused = managed->getPauseStatus();
  if (!was_paused) {
    holds_lock = managed->trySetPause(true);
    if (!holds_lock) {
      logMsg("CThread::ScopedLocker: failed to pause thread");
    }
  }
}

ScopedLocker::~ScopedLocker() {
  if (holds_lock) {
    managed->flush_request = true;
    if (!was_paused) {
      if (!managed->trySetPause(false)) {
        logMsg("CThread::ScopedLocker: failed to unpause thread");
      }
    } else {
      managed->requestStep();
    }
  }
}

CThread::CThread(PlayerContext& _ctx) : QThread(), ctx(_ctx) {}
CThread::~CThread() { joinOrTerminate(); }

bool CThread::joinOrTerminate() {
  bool success = true;

  if (isRunning()) {
    requestExit();
    if (!(success = wait(terminate_timeout))) {
      logMsg(
          "CThread error: Failed to normally quit from the thread, trying to "
          "terminate");
      terminate();
      if (!wait(terminate_timeout)) {
        logMsg("CThread error: failed to terminate");
      }
    }
  }

  return success;
}

bool CThread::trySetPause(bool _pause) {
  std::unique_lock lck(thr_lock);
  if (quit_requested) {
    return false;
  }

  auto success = false;
  if (_pause != is_paused) {
    is_paused = _pause;
    success = processRequest(lck);
  } else {
    logMsg((QString("CThread: thread is already ") +
            (_pause ? "paused" : "unpaused"))
               .toStdString());
  }

  return success;
}

void CThread::requestExit() {
  std::scoped_lock lck(thr_lock);
  quit_requested = true;
}

bool CThread::ensureThreadStart(int _timeout) {
  std::unique_lock lck(thr_lock);
  if (is_paused || quit_requested) {
    logMsg(
        "CThread::ensureThreadStart() is being used incorrectly. It is "
        "supposed to be called right after CThread::start(). Watch what you "
        "are doing!");
    return false;
  }

  is_paused = true;
  processRequest(lck, _timeout);
  is_paused = false;
  processRequest(lck, _timeout);

  return quit_requested;
}

bool CThread::processRequest(std::unique_lock<decltype(thr_lock)>& lck,
                             int _timeout) {
  const auto wait_start = qtplay::clk_now();
  while (!request_received) {
    if (_timeout > 0 &&
        (qtplay::clk_now() - wait_start) > qtplay::time_ms(_timeout)) {
      logMsg("CThread: failed to process request, timeout: %d", _timeout);
      break;
    }
    wait_cond.wait_for(lck, qtplay::time_ms(20));
  }

  const auto success = request_received;
  request_received = false;

  return success;
}

bool CThread::getPauseStatus() {
  std::scoped_lock lck(thr_lock);
  return is_paused;
}

bool CThread::eofReached() {
  std::scoped_lock lck(thr_lock);
  return thread_eof;
}

void CThread::requestStep() {
  std::scoped_lock lck(thr_lock);
  step_request = true;
}

bool CThread::start(bool ensure) {
  QThread::start();
  return ensure ? ensureThreadStart() : true;
}
