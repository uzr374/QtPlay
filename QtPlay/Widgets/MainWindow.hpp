#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QUrl>

class MainWindow final : public QMainWindow {
  Q_OBJECT;
  Q_DISABLE_COPY_MOVE(MainWindow);

  friend class QtPlayGUI;

 private:
  void setBestWindowGeometry();
  void closeEvent(QCloseEvent* evt) override;

  Q_SIGNAL void sigOpenURL(QUrl url);
  Q_SIGNAL void sigAddPlaylistEntry(QString url, QString title);

  Q_SLOT void handleAlwaysOnTop(bool ontop);
  Q_SLOT void triggerFileOpenDialog();

 public:
  MainWindow();
  ~MainWindow();
};
