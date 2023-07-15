#pragma once

#include <QDockWidget>

#include "../Visualizations/VisCommon.hpp"

class WaveDock final : public QDockWidget {
  Q_OBJECT;

 private:
  VisCommon* m_wWidget = nullptr;

 public:
  WaveDock(QWidget* parent);
  ~WaveDock();
};
