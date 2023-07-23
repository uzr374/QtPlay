#pragma once

#include <QDockWidget>
#include <QListWidget>
#include <QSettings>
#include <QBrush>
#include <QUrl>

class PlaylistItem final : public QListWidgetItem {
  Q_DISABLE_COPY_MOVE(PlaylistItem);

 private:
  QString title;
  QUrl url;
  bool is_playing = false;

 public:
  PlaylistItem(QListWidget* parent, const QUrl& url,
               const QString& title = QString());
  ~PlaylistItem();

  QUrl URL() const;
  QString URLStr() const;
  QString titleStr() const;
  void setPlayingState(bool playing);
  bool isPlaying() const;
};

class PlaylistWidget final : public QWidget {
  Q_OBJECT;
  Q_DISABLE_COPY_MOVE(PlaylistWidget);

 private:
  QListWidget* m_list = nullptr;
  const QString plEntriesG = "PlaylistEntries",
                plFilePath = "Settings/PlaylistConfig.ini",
                plValPrefix = plEntriesG;
  PlaylistItem* current_item = nullptr;
  int cur_item_row = -1;

private:
    Q_INVOKABLE void playNextImpl();

 public:
  PlaylistWidget(QWidget* parent);
  ~PlaylistWidget();

  Q_SLOT void addEntry(QUrl url, QString title);
  Q_SLOT void clearList();
  Q_SLOT void handleItemDoubleClick(QListWidgetItem* item);

  void loadList();
  void saveList();
  void unsetCurrentlyPlaying();
  void playNextItem();
  void playPrevItem();
  void setCurrentIndexFromItem(PlaylistItem* it);
  void playEntryByRow(int cur_row);

  //This function is thread-safe
  void playNext();

 private:
  QSettings getSettings() const;
};

class PlaylistDock final : public QDockWidget {
  Q_OBJECT;
  Q_DISABLE_COPY_MOVE(PlaylistDock);

 private:
  PlaylistWidget* m_playlist = nullptr;

 public:
  PlaylistDock(QWidget* parent);
  ~PlaylistDock();
};
