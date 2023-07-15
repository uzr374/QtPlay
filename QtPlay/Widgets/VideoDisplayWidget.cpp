#include "VideoDisplayWidget.hpp"

#include <QContextMenuEvent>
#include <QGridLayout>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>

// TODO: create and manage player instance in MainWindow class

VideoDisplayWidget::VideoDisplayWidget(QWidget* parent)
    : QWidget(parent), videoTimer(this), mouseTimer(this) {
  setObjectName("videoDisplayWidget");
  setWindowTitle(tr("Video"));

  setMouseTracking(true);
  setAttribute(Qt::WA_PaintOnScreen, true);
  setFocusPolicy(Qt::StrongFocus);
  setMinimumSize(QSize(100, 100));

  auto gLayout = new QGridLayout(this);
  gLayout->setContentsMargins(0, 0, 0, 0);
  videoWidget = new GLWindow(this);
  auto videoContainter = videoWidget->getWrapperWidget();
  videoContainter->installEventFilter(this);
  gLayout->addWidget(videoContainter);

  videoTimer.setInterval(50);
  videoTimer.setSingleShot(false);
  connect(&videoTimer, &QTimer::timeout, this,
          &VideoDisplayWidget::updateControls);
  connect(&mouseTimer, &QTimer::timeout, this, &VideoDisplayWidget::hideMouse);
  videoTimer.start();
}

VideoDisplayWidget::~VideoDisplayWidget() { closeStream(); }

static std::unique_ptr<PlayerContext> stream_open(
    QString filename, double audio_vol, std::vector<VisCommon*> viss) {
  try {
    return std::make_unique<PlayerContext>(filename.toStdString(), audio_vol,
                                           viss);
  } catch (...) {
    return nullptr;
  }
}

void VideoDisplayWidget::openURL(QUrl url) {
  closeStream();
  if (url.isValid()) {
    player_inst =
        stream_open(url.isLocalFile() ? url.toLocalFile() : url.toString(),
                    audio_vol, viss);
    if (!player_inst) {
      videoWidget->Invalidate();
    } else {
      /*waveW->startDisplay(player_inst);
      rdftW->startDisplay(player_inst);*/
    }
  }
}

void VideoDisplayWidget::closeStream() {
  player_inst = nullptr;

  if (videoWidget) {
    videoWidget->Invalidate();
    videoWidget->setOpened(false);
  }

  emit sigResetControls();
}

void VideoDisplayWidget::setVol(double percent) {
  audio_vol = percent * percent;
  if (player_inst) {
    player_inst->volume_percent = audio_vol;
  }
}

void VideoDisplayWidget::requestSeek(double percent) {
  if (player_inst) player_inst->seek_by_percent(percent);
}

void VideoDisplayWidget::paintEvent(QPaintEvent* evt) {
  QPainter p(this);
  p.fillRect(rect(), Qt::black);

  QWidget::paintEvent(evt);
}

void VideoDisplayWidget::contextMenuEvent(QContextMenuEvent* evt) {
  if (contextM && !contextM->isEmpty()) {
    contextM->exec(evt->globalPos());
  }
}

void VideoDisplayWidget::setContextMenu(QList<QMenu*> menu) {
  if (!menu.empty()) {
    if (!contextM) contextM = new QMenu(this);
    contextM->clear();
    for (auto const el : menu) {
      contextM->addMenu(el);
    }
  } else if (contextM) {
    contextM->clear();
  }
}

void VideoDisplayWidget::resizeEvent(QResizeEvent* evt) {
  QWidget::resizeEvent(evt);
}

void VideoDisplayWidget::updateControls() {
  if (player_inst) {
    const auto best_clock = player_inst->best_clkval();
    if (!std::isnan(best_clock)) {
      emit sigUpdatePlaybackPos(best_clock, player_inst->stream_duration.load(
                                                std::memory_order_relaxed));
    }
  }
}

void VideoDisplayWidget::handleKeyEvent(const Qt::Key key, bool autorepeat) {
  if (player_inst) {
    if (key == Qt::Key_Space) {
      player_inst->toggle_pause();
    } else if (key == Qt::Key_M) {
      player_inst->toggle_mute();
    } else if (key == Qt::Key_Right) {
      player_inst->seek_by_incr(5.0);
    } else if (key == Qt::Key_Left) {
      player_inst->seek_by_incr(-5.0);
    } else if (key == Qt::Key_A) {
      player_inst->request_stream_cycle(AVMEDIA_TYPE_AUDIO);
    }
  }
}

bool VideoDisplayWidget::event(QEvent* evt) {
  switch (evt->type()) {
    case QEvent::KeyPress: {
      auto kEvt = static_cast<const QKeyEvent*>(evt);
      handleKeyEvent((Qt::Key)kEvt->key(), kEvt->isAutoRepeat());
    } break;

    case QEvent::FocusIn:
    case QEvent::FocusOut:
    case QEvent::FocusAboutToChange:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
    case QEvent::Enter:
    case QEvent::Leave: {
      if (mouseTimer.isActive()) {
        mouseTimer.stop();
      }
      mouseTimer.setSingleShot(true);
      mouseTimer.setInterval(1000);
      setCursor(Qt::ArrowCursor);
      mouseTimer.start();
    } break;
  }

  return QWidget::event(evt);
}

void VideoDisplayWidget::setVisWidgets(VisCommon* wW, VisCommon* rW) {
  viss = {wW, rW};
}

void VideoDisplayWidget::pausePlayback() {
  if (player_inst && player_inst->read_tid) {
    const auto paused = player_inst->read_tid->getPauseStatus();
    if (!paused) {
      player_inst->toggle_pause();
    }
  }
}

void VideoDisplayWidget::resumePlayback() {
  if (player_inst && player_inst->read_tid) {
    const auto paused = player_inst->read_tid->getPauseStatus();
    if (paused) {
      player_inst->toggle_pause();
    }
  }
}

void VideoDisplayWidget::hideMouse() { setCursor(Qt::BlankCursor); }
