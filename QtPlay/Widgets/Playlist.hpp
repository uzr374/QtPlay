#pragma once

#include <QDockWidget>
#include <QListWidget>
#include <QSettings>
#include <QUrl>

class PlaylistItem final : public QListWidgetItem {
  Q_DISABLE_COPY_MOVE(PlaylistItem);

 private:
  QString title;
  QUrl url;

 public:
  PlaylistItem(QListWidget* parent, const QUrl& url,
               const QString& title = QString());
  ~PlaylistItem();

  QUrl URL() const;
  QString URLStr() const;
  QString titleStr() const;
};

class PlaylistWidget final : public QWidget {
  Q_OBJECT;
  Q_DISABLE_COPY_MOVE(PlaylistWidget);

 private:
  QListWidget* m_list = nullptr;
  const QString plEntriesG = "PlaylistEntries",
                plFilePath = "Settings/PlaylistConfig.ini",
                plValPrefix = plEntriesG;

 public:
  PlaylistWidget(QWidget* parent);
  ~PlaylistWidget();

  Q_SLOT void addEntry(QUrl url, QString title);
  Q_SLOT void clearList();
  Q_SLOT void handleItemDoubleClick(QListWidgetItem* item);

  Q_SIGNAL void sigOpenItem(QUrl url);

  void loadList();
  void saveList();

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
