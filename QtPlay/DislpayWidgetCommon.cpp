#include "DisplayWidgetCommon.hpp"

#include <QContextMenuEvent>
#include <QMenu>
#include <QPainter>

DisplayWidgetCommon::DisplayWidgetCommon(QWidget* p) : QWidget(p), mouseTimer(this) {
    setMouseTracking(true);
    setAttribute(Qt::WA_PaintOnScreen, true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(QSize(100, 100));

    connect(&mouseTimer, &QTimer::timeout, this, &DisplayWidgetCommon::hideMouse);
}
DisplayWidgetCommon::~DisplayWidgetCommon() {}

void DisplayWidgetCommon::hideMouse() {
	setCursor(Qt::BlankCursor);
}

void DisplayWidgetCommon::setContextMenu(QList<QMenu*> menu) {
    if (!menu.empty()) {
        if (!contextM) contextM = new QMenu(this);
        contextM->clear();
        for (auto const el : menu) {
            contextM->addMenu(el);
        }
    }
    else if (contextM) {
        contextM->clear();
    }
}

void DisplayWidgetCommon::paintEvent(QPaintEvent* evt) {
   /* QPainter p(this);
    p.fillRect(rect(), Qt::black);*/

    QWidget::paintEvent(evt);
}

void DisplayWidgetCommon::contextMenuEvent(QContextMenuEvent* evt) {
    if (contextM && !contextM->isEmpty()) {
        contextM->exec(evt->globalPos());
    }
}

bool DisplayWidgetCommon::event(QEvent* evt) {
    switch (evt->type()) {
    case QEvent::KeyPress: {
        auto kEvt = static_cast<const QKeyEvent*>(evt);
        //handleKeyEvent((Qt::Key)kEvt->key(), kEvt->isAutoRepeat());
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
        setCursor(Qt::ArrowCursor);
        if (mouseTimer.isActive()) {
            mouseTimer.stop();
        }
        mouseTimer.setSingleShot(true);
        mouseTimer.setInterval(1000);
        mouseTimer.start();
    } break;
    }

    return QWidget::event(evt);
}