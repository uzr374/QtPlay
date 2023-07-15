#include "WaveDock.hpp"

WaveDock::WaveDock(QWidget* parent) : QDockWidget(parent) {
  setObjectName("waveDock");
  setWindowTitle(tr("Waves display"));

  setAllowedAreas(Qt::AllDockWidgetAreas);

  m_wWidget = new VisCommon(this, VisCommon::VisType::WAVE);
  setWidget(m_wWidget);
}

WaveDock::~WaveDock() {}
