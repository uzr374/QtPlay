#include "MainWindow.hpp"

#include <QApplication>
#include <QFileDialog>
#include <QScreen>
#include <QSettings>
#include <QStyle>

#include "LoggerDock.hpp"
#include "MenuBar.hpp"
#include "Playlist.hpp"
#include "RDFTDock.hpp"
#include "StatusBar.hpp"
#include "ToolBar.hpp"
#include "VideoDisplayWidget.hpp"
#include "VideoDock.hpp"
#include "WaveDock.hpp"

MainWindow::MainWindow() : QMainWindow((QWidget*)nullptr) {
  setObjectName("mainWindow");
  setWindowTitle(tr("QtPlay"));
  setWindowIcon(style()->standardIcon(QStyle::SP_TitleBarMenuButton));

  auto cW = new QWidget;
  setCentralWidget(cW);

  setDockNestingEnabled(true);
  setAnimated(false);
  setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks);

  auto lgrD = new LoggerDock(this);
  addDockWidget(Qt::RightDockWidgetArea, lgrD);
  auto logger = qobject_cast<LoggerWidget*>(lgrD->widget());

  // Set up menu bar first
  auto mBar = new MenuBar;
  setMenuBar(mBar);

  auto statBar = new StatusBar;
  setStatusBar(statBar);

  auto tBar = new ToolBar(mBar->getBasicActions());
  addToolBar(Qt::BottomToolBarArea, tBar);

  auto videoD = new VideoDock(this, logger);
  addDockWidget(Qt::LeftDockWidgetArea, videoD);
  tabifyDockWidget(lgrD, videoD);

  auto videoW = qobject_cast<VideoDisplayWidget*>(videoD->widget());
  videoW->setContextMenu(mBar->getTopLevelMenus());

  auto waveD = new WaveDock(this);
  addDockWidget(Qt::RightDockWidgetArea, waveD);

  auto rdftD = new RDFTDock(this);
  addDockWidget(Qt::RightDockWidgetArea, rdftD);
  tabifyDockWidget(waveD, rdftD);

  auto plD = new PlaylistDock(this);
  addDockWidget(Qt::RightDockWidgetArea, plD);
  tabifyDockWidget(rdftD, plD);
  auto plW = qobject_cast<PlaylistWidget*>(plD->widget());

  cW->hide();  // Hide placeholder central widget

  // Set up all inter-widgets connections after UI setup
  connect(mBar, &MenuBar::sigAlwaysOnTopToggled, this,
          &MainWindow::handleAlwaysOnTop);
  connect(mBar, &MenuBar::sigOpenFileTriggered, this,
          &MainWindow::triggerFileOpenDialog);
  connect(mBar, &MenuBar::sigCloseStreamTriggered, videoW,
          &VideoDisplayWidget::closeStream);
  connect(mBar, &MenuBar::sigPausePlayback, videoW,
          &VideoDisplayWidget::pausePlayback);
  connect(mBar, &MenuBar::sigResumePlayback, videoW,
          &VideoDisplayWidget::resumePlayback);
  connect(mBar, &MenuBar::sigClearPlaylist, plW, &PlaylistWidget::clearList);

  connect(tBar, &ToolBar::sigNewVol, videoW, &VideoDisplayWidget::setVol);
  connect(tBar, &ToolBar::sigReqSeek, videoW, &VideoDisplayWidget::requestSeek);

  connect(videoW, &VideoDisplayWidget::sigUpdatePlaybackPos, tBar,
          &ToolBar::updatePlaybackPos);
  connect(videoW, &VideoDisplayWidget::sigUpdatePlaybackPos, statBar,
          &StatusBar::updatePlaybackPos);
  connect(videoW, &VideoDisplayWidget::sigResetControls, tBar,
          &ToolBar::resetPlaybackPos);
  connect(videoW, &VideoDisplayWidget::sigResetControls, statBar,
          &StatusBar::resetPlaybackPos);

  connect(this, &MainWindow::sigOpenURL, videoW, &VideoDisplayWidget::openURL);
  connect(this, &MainWindow::sigAddPlaylistEntry, plW,
          &PlaylistWidget::addEntry);
  connect(plW, &PlaylistWidget::sigOpenItem, videoW,
          &VideoDisplayWidget::openURL);

  // Set window geometry only after UI setup is finished
  setBestWindowGeometry();
  videoW->setVol(tBar->getVolumePercent());
  videoW->setVisWidgets(qobject_cast<VisCommon*>(waveD->widget()),
                        qobject_cast<VisCommon*>(rdftD->widget()));
}

MainWindow::~MainWindow() {
  QSettings sets("Settings/MainWindow.ini", QSettings::IniFormat);
  sets.setValue("MainWindow/WindowState", saveState());
  sets.setValue("MainWindow/WindowGeom", geometry());
  sets.setValue("MainWindow/OnTop",
                bool(windowFlags() & Qt::WindowStaysOnTopHint));
}

void MainWindow::setBestWindowGeometry() {
  constexpr auto scale_fact = 0.4;
  auto curScr = screen();
  const auto scr_geom = curScr->geometry();
  const auto dstW = scr_geom.width() * scale_fact;
  const auto dstH = scr_geom.height() * scale_fact;
  const auto topLeftX = (scr_geom.width() - dstW) / 2;
  const auto topLeftY = (scr_geom.height() - dstH) / 2;
  setMinimumWidth(100);
  setMinimumHeight(100);
  const QRect recommendedGeom(topLeftX, topLeftY, dstW, dstH);
  setGeometry(recommendedGeom);
  const auto default_state = saveState();
  QSettings sets("Settings/MainWindow.ini", QSettings::IniFormat);
  const auto saved_state =
      sets.value("MainWindow/WindowState", default_state).toByteArray();
  restoreState(saved_state);

  // Hide floating docks
  for (auto w : QApplication::allWidgets()) {
    if (auto dW = qobject_cast<QDockWidget*>(w)) {
      if (dW->isFloating()) {
        dW->setFloating(false);
      }
    }
  }

  const auto saved_geom =
      sets.value("MainWindow/WindowGeom", recommendedGeom).toRect();
  if (scr_geom.contains(saved_geom)) {
    setGeometry(saved_geom);
  } else {
    setGeometry(recommendedGeom);
  }

  const auto on_top = sets.value("MainWindow/OnTop", false).toBool();
  if (on_top) {
    qobject_cast<MenuBar*>(menuBar())->alwaysOnTopAct->toggle();
  }
}

void MainWindow::handleAlwaysOnTop(bool ontop) {
  const auto wFlags = windowFlags();
  const auto on_top_bit = Qt::WindowStaysOnTopHint;
  const bool currently_on_top = (wFlags & on_top_bit);
  if (currently_on_top != ontop) {
    setWindowFlags((ontop ? (wFlags | on_top_bit) : (wFlags & ~on_top_bit)));
    show();  // setWindowFlags() hides window, so show it
  }
}

void MainWindow::triggerFileOpenDialog() {
  const auto url = QFileDialog::getOpenFileUrl(this, tr("Open file"));
  if (!url.isEmpty()) {
    const auto strUrl = url.toString();
    emit sigAddPlaylistEntry(strUrl, QString());
    emit sigOpenURL(url);
  }
}
