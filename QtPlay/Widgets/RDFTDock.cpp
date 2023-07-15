#include "RDFTDock.hpp"

RDFTDock::RDFTDock(QWidget* parent) : QDockWidget(parent) {
  setObjectName("rdftDock");
  setWindowTitle(tr("RDFT display"));

  setAllowedAreas(Qt::AllDockWidgetAreas);

  m_wWidget = new VisCommon(this, VisCommon::VisType::RDFT);
  setWidget(m_wWidget);
}

RDFTDock::~RDFTDock() {}
