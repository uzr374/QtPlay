#pragma once

#include <QDockWidget>

#include "LoggerWidget.hpp"

class LoggerDock final : public QDockWidget {
  Q_OBJECT;

 private:
  LoggerWidget* m_logger = nullptr;

 public:
  LoggerDock(QWidget* parent);
  ~LoggerDock();
};
