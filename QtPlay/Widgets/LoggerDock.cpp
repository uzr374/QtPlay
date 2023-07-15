#include "LoggerDock.hpp"

LoggerDock::LoggerDock(QWidget* parent) : QDockWidget(parent) {
  setObjectName("loggerDock");
  setWindowTitle(tr("Logger output"));

  m_logger = new LoggerWidget(this);
  setWidget(m_logger);
}

LoggerDock::~LoggerDock() {}
