#include "ToolBar.hpp"

#include <QSettings>

#include "CSlider.hpp"

ToolBar::ToolBar(QList<QAction*> acts) : QToolBar(nullptr) {
  setObjectName("toolBar");
  setWindowTitle(tr("toolBar"));

  for (auto act : acts) {
    addAction(act);
    addSeparator();
  }

  volSlider = new CSlider(nullptr);
  volSlider->setObjectName("volSlider");
  volSlider->setWindowTitle(tr("Volume slider"));
  volSlider->setMaximumWidth(110);

  playSlider = new CSlider(nullptr);
  playSlider->setObjectName("playSlider");
  playSlider->setWindowTitle(tr("Playback slider"));
  playSlider->setEnabled(false);

  addWidget(playSlider);
  addSeparator();
  addWidget(volSlider);

  connect(volSlider, &CSlider::sigValChanged, this,
          &ToolBar::handleSliderVolumeChange);
  connect(playSlider, &CSlider::sigValChanged, this, &ToolBar::sigReqSeek);

  QSettings sets("Settings/ToolBar.ini", QSettings::IniFormat);
  vol_percent = sets.value("volume", 1.0).toDouble();
  volSlider->setValue(vol_percent * volSlider->maximum());
}

ToolBar::~ToolBar() {
  QSettings sets("Settings/ToolBar.ini", QSettings::IniFormat);
  sets.setValue("volume", vol_percent);
}

void ToolBar::handleSliderVolumeChange(double vol) {
  vol_percent = vol;
  emit sigNewVol(vol);
}

void ToolBar::updatePlaybackPos(double elapsed, double duration) {
  const auto percent = duration > 0.0 ? elapsed / duration : 0.0;
  if (!playSlider->isEnabled()) playSlider->setEnabled(true);
  playSlider->setPositionPercent(percent);
}

void ToolBar::resetPlaybackPos() {
  updatePlaybackPos(0.0, 0.0);
  if (playSlider->isEnabled()) playSlider->setEnabled(false);
}

double ToolBar::getVolumePercent() const { return vol_percent; }
