#pragma once

#include <QMainWindow>
#include <QUrl>

class MainWindow final : public QMainWindow {
  Q_OBJECT;
  Q_DISABLE_COPY_MOVE(MainWindow);

 private:
  void setBestWindowGeometry();

  Q_SIGNAL void sigOpenURL(QUrl url);
  Q_SIGNAL void sigAddPlaylistEntry(QString url, QString title);

  Q_SLOT void handleAlwaysOnTop(bool ontop);
  Q_SLOT void triggerFileOpenDialog();

 public:
  MainWindow();
  ~MainWindow();
};
