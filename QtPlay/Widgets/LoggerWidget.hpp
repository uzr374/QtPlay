#pragma once

#include <QTextEdit>

class LoggerWidget final : public QWidget {
  Q_OBJECT;
  Q_DISABLE_COPY_MOVE(LoggerWidget);

 private:
  QTextEdit* m_edit = nullptr;

  Q_SIGNAL void sigLogMsg(QString msg);
  Q_SLOT void logMsg_priv(QString msg);

 public:
  explicit LoggerWidget(QWidget* parent);
  ~LoggerWidget();

  /* Thread-safe */
  void emitMsg(const QString& msg);
};
