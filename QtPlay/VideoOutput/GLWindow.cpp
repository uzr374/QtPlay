#include "GLWindow.hpp"

#include <QCoreApplication>
#include <QThread>
#include <QWidget>

GLWindow::GLWindow(QWidget* parent, Qt::WindowFlags flags) : QOpenGLWindow() {
  setObjectName("OpenGLWindow");
  setFlag(Qt::WindowTransparentForInput);

  m_wrapperWidget = QWidget::createWindowContainer(this, parent, flags);
  m_wrapperWidget->setAcceptDrops(false);
  m_wrapperWidget->setMouseTracking(true);
  m_wrapperWidget->setFocusPolicy(Qt::StrongFocus);
  m_wrapperWidget->setAttribute(Qt::WA_NativeWindow, true);
  m_wrapperWidget->setAttribute(Qt::WA_OpaquePaintEvent, true);
  m_wrapperWidget->setAttribute(Qt::WA_PaintOnScreen, true);
  m_wrapperWidget->setAttribute(Qt::WA_NoSystemBackground, true);
}

GLWindow::~GLWindow() {
  makeCurrent();
  GLCommon::reset();
  doneCurrent();
}

void GLWindow::resizeGL(int newW, int newH) { GLCommon::onResize(newW, newH); }

void GLWindow::initializeGL() { GLCommon::doInitGL(); }

void GLWindow::paintGL() {
  if (isExposed()) GLCommon::doUpdateGL();
}

void GLWindow::Invalidate() {
  setOpened(false);
  makeCurrent();
  GLCommon::reset();
  doneCurrent();
  requestUpdate(false);
}

bool GLWindow::event(QEvent* e) {
  switch (e->type()) {
      /*Thanks to QMPlay2 for this piece of code*/
      /*Pass through all events we want parent widget(s) to handle*/
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseMove:
    case QEvent::WindowActivate:
    case QEvent::WindowDeactivate:
    case QEvent::FocusIn:
    case QEvent::FocusOut:
    case QEvent::FocusAboutToChange:
    case QEvent::Enter:
    case QEvent::Leave:
    case QEvent::TabletMove:
    case QEvent::TabletPress:
    case QEvent::TabletRelease:
    case QEvent::TabletEnterProximity:
    case QEvent::TabletLeaveProximity:
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::InputMethodQuery:
    case QEvent::TouchCancel:
      QCoreApplication::sendEvent(parent(), e);
      break;
  }

  return QOpenGLWindow::event(e);
}

void GLWindow::zoom(int sign) {
  GLCommon::zoom(sign);
  requestUpdate(false);
}

void GLWindow::removeOSD(bool queued) {
  GLCommon::removeOSD();
  requestUpdate(queued);
}

void GLWindow::requestUpdate(bool queued) {
  return queued ? QCoreApplication::postEvent(this,
                                              new QEvent(QEvent::UpdateRequest),
                                              Qt::HighEventPriority)
                : update();
}
