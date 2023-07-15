#include "LoggerWidget.hpp"

#include <QGridLayout>

#include "../Common/QtPlayCommon.hpp"

static LoggerWidget* m_inst = nullptr;

LoggerWidget::LoggerWidget(QWidget* parent) : QWidget(parent) {
  m_edit = new QTextEdit(this);
  m_edit->setReadOnly(true);
  auto lout = new QGridLayout(this);
  lout->addWidget(m_edit);

  m_inst = this;  // There will be only one instance in the entire system,
                  // initialize it

  connect(this, &LoggerWidget::sigLogMsg, this, &LoggerWidget::logMsg_priv);
}

LoggerWidget::~LoggerWidget() {}

void LoggerWidget::emitMsg(const QString& msg) { emit sigLogMsg(msg); }

void LoggerWidget::logMsg_priv(QString msg) { m_edit->append(msg); }

void qtplay::logMsg(QAnyStringView fmt, ...) {
  va_list arglist;
  va_start(arglist, fmt);
  m_inst->emitMsg(QString::vasprintf(qtplay::ptr_cast<const char>(fmt.data()), arglist));
  va_end(arglist);
}
