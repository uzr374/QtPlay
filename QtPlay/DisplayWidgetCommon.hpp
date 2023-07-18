#pragma once

#include <QWidget>
#include <QTimer>

class DisplayWidgetCommon : public QWidget {
	Q_OBJECT;
	Q_DISABLE_COPY_MOVE(DisplayWidgetCommon);

private:
	QMenu* contextM = nullptr;
	QTimer mouseTimer;

private:
	void contextMenuEvent(QContextMenuEvent* evt) override;
	void paintEvent(QPaintEvent* evt) override;
	bool event(QEvent* evt) override;

public:
	explicit DisplayWidgetCommon(QWidget* parent);
	virtual ~DisplayWidgetCommon();

	Q_SLOT void setContextMenu(QList<QMenu*> menu);
	Q_SLOT void hideMouse();

};