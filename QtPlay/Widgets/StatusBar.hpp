#pragma once

#include <QStatusBar>

class StatusBar final : public QStatusBar {
  Q_OBJECT;
  Q_DISABLE_COPY_MOVE(StatusBar);

 private:
  class QLabel* timeLbl = nullptr;

 public:
  StatusBar();
  ~StatusBar();

  Q_SLOT void updatePlaybackPos(double elapsed, double duration);
  Q_SLOT void resetPlaybackPos();
};
