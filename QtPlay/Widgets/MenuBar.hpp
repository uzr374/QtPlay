#pragma once

#include <QMenu>
#include <QMenuBar>

class MenuBar final : public QMenuBar {
  Q_OBJECT;
  Q_DISABLE_COPY_MOVE(MenuBar);

 public:
  QMenu *fileMenu = nullptr, *viewMenu = nullptr, *playbackMenu = nullptr,
        *pllistMenu = nullptr;
  QAction* fileOpenAct = nullptr, * stopPlaybackAct = nullptr,
      * alwaysOnTopAct = nullptr, * pauseAct = nullptr, * resumeAct = nullptr,
      * playlistClearAct = nullptr, * playNextAct = nullptr, * playPrevAct = nullptr;

  QList<QMenu*> m_topLevelMenus;
  QMenu context_menu; 

 public:
  MenuBar();
  ~MenuBar();

  Q_SIGNAL void sigOpenFileTriggered();
  Q_SIGNAL void sigCloseStreamTriggered();
  Q_SIGNAL void sigAlwaysOnTopToggled(bool ontop);
  Q_SIGNAL void sigPausePlayback();
  Q_SIGNAL void sigResumePlayback();
  Q_SIGNAL void sigClearPlaylist();

  Q_SLOT void setAlwaysOnTop(bool ontop);

  QList<QAction *> getBasicActions();
  QList<QMenu *> getTopLevelMenus();
  QMenu& getContextMenu();
};
