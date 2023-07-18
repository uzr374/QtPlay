#pragma once

#include <QObject>

class PlaybackEventFilter final : public QObject {
	Q_OBJECT;
	Q_DISABLE_COPY_MOVE(PlaybackEventFilter);

private:
	bool eventFilter(QObject* watched, QEvent* evt) override;

public:
	explicit PlaybackEventFilter(QObject* parent);
	~PlaybackEventFilter();

};