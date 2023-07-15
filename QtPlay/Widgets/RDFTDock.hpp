#pragma once

#include <QDockWidget>

#include "../Visualizations/VisCommon.hpp"

class RDFTDock final : public QDockWidget {
  Q_OBJECT;

 private:
  VisCommon* m_wWidget = nullptr;

 public:
  RDFTDock(QWidget* parent);
  ~RDFTDock();
};
