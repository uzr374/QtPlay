#include "VideoDock.hpp"

VideoDock::VideoDock(QWidget* parent, LoggerWidget* logger)
    : QDockWidget(parent) {
  setObjectName("videoDock");
  setWindowTitle(tr("Video"));

  setAllowedAreas(Qt::AllDockWidgetAreas);

  vW = new VideoDisplayWidget(this);
  setWidget(vW);
}

VideoDock::~VideoDock() {}
