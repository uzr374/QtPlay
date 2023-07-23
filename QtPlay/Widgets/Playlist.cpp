#include "Playlist.hpp"

#include "../PlayerCore.hpp"

#include <QGridLayout>

PlaylistItem::PlaylistItem(QListWidget* parent, const QUrl& _url,
                           const QString& _title)
    : QListWidgetItem(parent) {
  this->url = _url;
  this->title = _title;

  if (title.isEmpty()) {
    title = url.isLocalFile() ? url.fileName() : url.toString();
  }

  setToolTip(title);
  setText(title);
}

PlaylistItem::~PlaylistItem() {}

QUrl PlaylistItem::URL() const { return url; }

QString PlaylistItem::URLStr() const { return url.toString(); }

QString PlaylistItem::titleStr() const { return title; }

void PlaylistItem::setPlayingState(bool playing) {
    const static QBrush playing_color = QBrush(QColor(255, 191, 128)), default_color = QBrush();
    is_playing = playing;
    setBackground(is_playing ? playing_color : default_color);
}

bool PlaylistItem::isPlaying() const {
    return is_playing;
}

PlaylistWidget::PlaylistWidget(QWidget* parent) {
  setObjectName("playlistWidget");
  setWindowTitle(tr("Playlist"));
  setAccessibleDescription(tr("Playlist widget"));

  auto gL = new QGridLayout(this);
  m_list = new QListWidget(this);
  m_list->setObjectName("plTree");

  gL->addWidget(m_list, 0, 0, 100, 100);

  connect(m_list, &QListWidget::itemDoubleClicked, this,
          &PlaylistWidget::handleItemDoubleClick);

  loadList();
}

PlaylistWidget::~PlaylistWidget() { saveList(); }

QSettings PlaylistWidget::getSettings() const {
  return QSettings(plFilePath, QSettings::IniFormat);
}

void PlaylistWidget::saveList() {
  auto plSets(getSettings());
  plSets.beginGroup(plEntriesG);
  plSets.beginWriteArray(plValPrefix);
  for (auto i = 0; i < m_list->count(); ++i) {
    plSets.setArrayIndex(i);
    auto item = static_cast<const PlaylistItem*>(m_list->item(i));
    plSets.setValue("URL", item->URL());
    plSets.setValue("Title", item->titleStr());
  }
  plSets.endArray();
  plSets.endGroup();
}

void PlaylistWidget::loadList() {
  auto plSets(getSettings());
  plSets.beginGroup(plEntriesG);
  const auto count = plSets.beginReadArray(plValPrefix);
  for (auto i = 0; i < count; ++i) {
    plSets.setArrayIndex(i);
    const auto url = plSets.value("URL", QUrl()).toUrl();
    const auto title = plSets.value("Title", QString()).toString();
    addEntry(url, title);
  }
  plSets.endArray();
  plSets.endGroup();
}

void PlaylistWidget::addEntry(QUrl url, QString title) {
  if (!url.isEmpty()) {
    auto newItem = new PlaylistItem(m_list, url, title);
    m_list->addItem(newItem);
  }
}

void PlaylistWidget::clearList() {
  m_list->clear();
  saveList();
}

void PlaylistWidget::unsetCurrentlyPlaying() {
    if (current_item) {
        current_item->setPlayingState(false);
        current_item = nullptr;
    }
}

void PlaylistWidget::handleItemDoubleClick(QListWidgetItem* it) {
    unsetCurrentlyPlaying();
  auto entry = static_cast<PlaylistItem*>(it);
  setCurrentIndexFromItem(entry);
  try {
      playerCore.openURL(entry->URL());
  }
  catch (...) {
      return;
  }
  
  entry->setPlayingState(true);
  current_item = entry;
}

void PlaylistWidget::setCurrentIndexFromItem(PlaylistItem* it) {
    if (!it) {
        cur_item_row = -1;
    }
    else {
        cur_item_row = m_list->row(it);
    }
    m_list->setCurrentRow(cur_item_row);
}

void  PlaylistWidget::playEntryByRow(int cur_row) {
    if (cur_row < 0 || cur_row >= m_list->count() - 1) return;
    cur_item_row = cur_row;
    auto cur_item = static_cast<PlaylistItem*>(m_list->item(cur_item_row));
    handleItemDoubleClick(cur_item);
}

void PlaylistWidget::playPrevItem() {
    const auto count = m_list->count();
    if (cur_item_row <= 0 || count <= 0) return;
    auto next_row = --cur_item_row;
    playEntryByRow(next_row);
}

void PlaylistWidget::playNextItem() {
    const auto count = m_list->count();
    if (count <= 0 || cur_item_row >= count - 1) return;
    auto next_row = ++cur_item_row;
    playEntryByRow(next_row);
}

void PlaylistWidget::playNext() {
    QMetaObject::invokeMethod(this, "playNextImpl", Qt::QueuedConnection);
}

void  PlaylistWidget::playNextImpl() {
    playNextItem();
}


PlaylistDock::PlaylistDock(QWidget* parent) : QDockWidget(parent) {
  setObjectName("playlistDock");

  m_playlist = new PlaylistWidget(this);
  setWidget(m_playlist);

  setWindowTitle(m_playlist->windowTitle());
  setAccessibleDescription(m_playlist->accessibleDescription());
}

PlaylistDock::~PlaylistDock() {}
