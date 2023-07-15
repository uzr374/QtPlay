#pragma once

#include <QThread>
#include <condition_variable>
#include <mutex>

class CThread : public QThread {
  Q_OBJECT;
  CThread() = delete;

 protected:
  struct PlayerContext& ctx;

  constexpr static int terminate_timeout =
      4900;  // Terminate the thread before OS will
             // think that the application is frozen
  std::mutex thr_lock;
  // Protected by the thr_lock
  std::condition_variable wait_cond;
  bool is_paused = false, step_request = false, flush_request = false,
       quit_requested = false, request_received = false, thread_eof = false;

 public:
  class ScopedLocker final {
    friend class CThread;
    ScopedLocker() = delete;
    Q_DISABLE_COPY_MOVE(ScopedLocker);

   private:
    bool holds_lock = false, was_paused = false;
    std::unique_ptr<CThread>& managed;

   public:
    [[nodiscard]] explicit ScopedLocker(std::unique_ptr<CThread>& _thr);
    ~ScopedLocker();

    inline bool holdsLock() const { return holds_lock; }
  };

  [[nodiscard]] explicit CThread(PlayerContext&);
  virtual ~CThread();

  [[nodiscard]] bool joinOrTerminate();
  void requestExit();
  [[nodiscard]] bool trySetPause(bool _paused);
  [[nodiscard]] bool getPauseStatus();
  [[nodiscard]] bool eofReached();
  void requestStep();
  bool start(bool ensure = true);

 private:
  [[nodiscard]] bool processRequest(std::unique_lock<decltype(thr_lock)>& lck,
                                    int _timeout = terminate_timeout);
  [[nodiscard]] bool ensureThreadStart(int timeout = terminate_timeout);
};
