#pragma once

#include <QMenu>
#include <QTimer>
#include <QUrl>
#include <QWidget>
#include <memory>

#include "../Common/PlayerContext.hpp"
#include "RDFTDock.hpp"
#include "WaveDock.hpp"

class VideoDisplayWidget final : public QWidget {
  Q_OBJECT;
  Q_DISABLE_COPY_MOVE(VideoDisplayWidget);

 private:
  void paintEvent(QPaintEvent* evt) override;
  void contextMenuEvent(QContextMenuEvent* evt) override;
  void resizeEvent(QResizeEvent* evt) override;
  bool event(QEvent* evt) override;
  void handleKeyEvent(const Qt::Key key, bool autorepeat);

 private:
  QMenu* contextM = nullptr;
  QTimer videoTimer, mouseTimer;
  double audio_vol = 1.0;

  std::unique_ptr<PlayerContext> player_inst = nullptr;
  std::vector<VisCommon*> viss;

 public:
  VideoDisplayWidget(QWidget* parent);
  ~VideoDisplayWidget();

  Q_SIGNAL void sigUpdatePlaybackPos(double elapsed, double duration);
  Q_SIGNAL void sigResetControls();

  Q_SLOT void openURL(QUrl url);
  Q_SLOT void closeStream();
  Q_SLOT void setVol(double percent);
  Q_SLOT void requestSeek(double percent);
  Q_SLOT void setContextMenu(QList<QMenu*> menu);
  Q_SLOT void updateControls();
  Q_SLOT void pausePlayback();
  Q_SLOT void resumePlayback();
  Q_SLOT void hideMouse();

  void setVisWidgets(VisCommon* wW, VisCommon* rW);
};
