#include "MenuBar.hpp"

#include <QApplication>
#include <QStyle>

#include "VideoDisplayWidget.hpp"

constexpr auto fileopen_iconname = QStyle::SP_FileDialogStart,
               pause_iconname = QStyle::SP_MediaPause,
               resume_iconname = QStyle::SP_MediaPlay,
               playbackstop_iconname = QStyle::SP_MediaStop, playlistclear_iconname = QStyle::SP_TrashIcon;

auto getIcon(QStyle::StandardPixmap type) {
  auto style = QApplication::style();
  return style->standardIcon(type);
}

MenuBar::MenuBar() : QMenuBar() {
  setObjectName("menuBar");
  setWindowTitle(tr("Menu bar"));

  // Top-level menus
  fileMenu = addMenu(tr("File"));
  viewMenu = addMenu(tr("View"));
  playbackMenu = addMenu(tr("Playback"));
  pllistMenu = addMenu(tr("Playlist"));

  m_topLevelMenus << fileMenu << viewMenu << playbackMenu << pllistMenu;

  //"File" menu actions
  fileOpenAct = fileMenu->addAction(tr("Open file"));

  fileOpenAct->setIcon(getIcon(fileopen_iconname));

  //"View" menu actions
  alwaysOnTopAct = viewMenu->addAction(tr("Always on top"));
  alwaysOnTopAct->setCheckable(true);
  alwaysOnTopAct->setChecked(false);
  alwaysOnTopAct->setShortcut(Qt::CTRL + Qt::Key_T);

  //"Playback" menu actions
  pauseAct = playbackMenu->addAction(tr("Pause"));
  resumeAct = playbackMenu->addAction(tr("Resume"));
  stopPlaybackAct = playbackMenu->addAction(tr("Stop"));

  pauseAct->setIcon(getIcon(pause_iconname));
  resumeAct->setIcon(getIcon(resume_iconname));
  stopPlaybackAct->setIcon(getIcon(playbackstop_iconname));

  // Playlist actions
  playlistClearAct = pllistMenu->addAction(tr("Clear list"));

  playlistClearAct->setIcon(getIcon(playlistclear_iconname));

  for (auto mnu : m_topLevelMenus) {
    context_menu.addMenu(mnu);
  }

  // Make connections
  connect(alwaysOnTopAct, &QAction::toggled, this,
          &MenuBar::sigAlwaysOnTopToggled);
  connect(fileOpenAct, &QAction::triggered, this,
          &MenuBar::sigOpenFileTriggered);
  connect(stopPlaybackAct, &QAction::triggered, this,
          &MenuBar::sigCloseStreamTriggered);
  connect(pauseAct, &QAction::triggered, this, &MenuBar::sigPausePlayback);
  connect(resumeAct, &QAction::triggered, this, &MenuBar::sigResumePlayback);
  connect(playlistClearAct, &QAction::triggered, this,
          &MenuBar::sigClearPlaylist);
}

MenuBar::~MenuBar() {}

QMenu& MenuBar::getContextMenu() { return context_menu; }

void MenuBar::setAlwaysOnTop(bool ontop) { alwaysOnTopAct->setChecked(ontop); }

QList<QAction*> MenuBar::getBasicActions() {
  return QList<QAction*>({pauseAct, resumeAct, stopPlaybackAct});
}

QList<QMenu*> MenuBar::getTopLevelMenus() {
  return QList<QMenu*>({fileMenu, viewMenu, playbackMenu});
}
