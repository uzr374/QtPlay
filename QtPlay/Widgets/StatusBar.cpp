#include "StatusBar.hpp"

#include <QLabel>

StatusBar::StatusBar() : QStatusBar(nullptr) {
  setObjectName("statusBar");
  setWindowTitle(tr("statusBar"));

  timeLbl = new QLabel;
  insertPermanentWidget(0, timeLbl);
  updatePlaybackPos(0.0, 0.0);
}

StatusBar::~StatusBar() {}

void StatusBar::updatePlaybackPos(double elapsed, double duration) {
  auto timeToStr = [](double seconds) {
    QString res;
    if (!std::isnan(seconds)) {
      const int hours = seconds / 3600;
      const int mins = ((int64_t)seconds % 3600) / 60;
      const int secs = ((int64_t)seconds % 60);
      const std::string spec = "%d", spec_z = "0%d";
      std::string fmt;
      if (hours) {
        fmt += spec + ":";
      }

      fmt += (std::abs(mins) < 10 ? spec_z : spec) + ":";
      fmt += (std::abs(secs) < 10 ? spec_z : spec);

      if (hours)
        res = QString::asprintf(fmt.c_str(), hours, mins, secs);
      else
        res = QString::asprintf(fmt.c_str(), mins, secs);
    } else {
      res = "00:00";
    }

    return res;
  };

  timeLbl->setText(timeToStr(elapsed) + QString(" | -") +
                   timeToStr(duration > 0 ? (duration - elapsed) : 0.0) +
                   QString(" / ") + timeToStr(duration));
}

void StatusBar::resetPlaybackPos() { updatePlaybackPos(0.0, 0.0); }
