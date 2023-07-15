#pragma once

#include "../Common/CThread.hpp"

class AudioThread final : public CThread {
  Q_OBJECT;
  AudioThread() = delete;

 private:
  void run() override;

 public:
  AudioThread(PlayerContext& _ctx);
  ~AudioThread();
};
