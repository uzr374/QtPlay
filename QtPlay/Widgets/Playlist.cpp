#include "Playlist.hpp"

#include <QGridLayout>

PlaylistItem::PlaylistItem(QListWidget* parent, const QUrl& _url,
                           const QString& _title)
    : QListWidgetItem(parent) {
  this->url = _url;
  this->title = _title;

  if (title.isEmpty()) {
    title = url.isLocalFile() ? url.fileName() : url.toString();
  }

  setToolTip(url.toString());
  setText(title);
}

PlaylistItem::~PlaylistItem() {}

QUrl PlaylistItem::URL() const { return url; }

QString PlaylistItem::URLStr() const { return url.toString(); }

QString PlaylistItem::titleStr() const { return title; }

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

void PlaylistWidget::handleItemDoubleClick(QListWidgetItem* it) {
  auto entry = static_cast<PlaylistItem*>(it);
  emit sigOpenItem(QUrl(entry->URLStr()));
}

PlaylistDock::PlaylistDock(QWidget* parent) : QDockWidget(parent) {
  setObjectName("playlistDock");

  m_playlist = new PlaylistWidget(this);
  setWidget(m_playlist);

  setWindowTitle(m_playlist->windowTitle());
  setAccessibleDescription(m_playlist->accessibleDescription());
}

PlaylistDock::~PlaylistDock() {}
