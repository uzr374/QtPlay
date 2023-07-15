#pragma once

#include "../Common/CThread.hpp"

class DemuxThread final : public CThread {
  Q_OBJECT;
  DemuxThread() = delete;

 private:
  void run() override;

  std::atomic_bool abort_demuxer = false;

 public:
  DemuxThread(PlayerContext& _ctx);
  ~DemuxThread();
};
