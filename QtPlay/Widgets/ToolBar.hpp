#pragma once

#include <QToolBar>

#include "CSlider.hpp"

class ToolBar final : public QToolBar {
  Q_OBJECT;
  Q_DISABLE_COPY_MOVE(ToolBar);

 private:
  CSlider *playSlider = nullptr, *volSlider = nullptr;
  double vol_percent = 1.0;

  Q_SLOT void handleSliderVolumeChange(double vol);

 public:
  ToolBar(QList<QAction *> acts);
  ~ToolBar();

  Q_SIGNAL void sigNewVol(double vol);
  Q_SIGNAL void sigReqSeek(double percent);

  Q_SLOT void updatePlaybackPos(double elapsed, double duration);
  Q_SLOT void resetPlaybackPos();

  double getVolumePercent() const;
};
