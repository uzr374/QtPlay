#pragma once
#include "../Common/CThread.hpp"

class VideoThread final : public CThread {
  Q_OBJECT;
  VideoThread() = delete;
  Q_DISABLE_COPY_MOVE(VideoThread);

 private:
  void run() override;

 public:
  VideoThread(PlayerContext& _ctx);
  ~VideoThread();
};
