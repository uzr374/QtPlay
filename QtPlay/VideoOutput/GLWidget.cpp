#include "GLWidget.hpp"

#include <QCoreApplication>

GLWidget::GLWidget(QWidget* parent) : QOpenGLWidget(parent) {
  setObjectName("OpenGLWidget");
  setAcceptDrops(false);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
}

GLWidget::~GLWidget() {
  makeCurrent();
  GLCommon::reset();
  doneCurrent();
}

void GLWidget::resizeGL(int newW, int newH) { GLCommon::onResize(newW, newH); }

void GLWidget::initializeGL() { GLCommon::doInitGL(); }

void GLWidget::paintGL() { GLCommon::doUpdateGL(); }

void GLWidget::zoom(int sign) {
  GLCommon::zoom(sign);
  update();
}

void GLWidget::sendVideoData(const Frame& fr, bool queued) {
  GLCommon::setVideoData(fr);
  requestUpdate(queued);
}

Frame GLWidget::getCurrentFrame() { return Frame(); }

void GLWidget::setOSD(const Frame& osd, bool queued) {
  GLCommon::setOSDImage(osd);
  requestUpdate(queued);
}

void GLWidget::removeOSD(bool queued) {
  GLCommon::removeOSD();
  requestUpdate(queued);
}

void GLWidget::requestUpdate(bool queued) {
  QMetaObject::invokeMethod(this, "update",
                            queued ? Qt::QueuedConnection : Qt::AutoConnection);
}

void GLWidget::Invalidate() {
  makeCurrent();
  GLCommon::reset();
  doneCurrent();
  requestUpdate(false);
}
