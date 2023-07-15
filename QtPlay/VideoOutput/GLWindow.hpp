#pragma once

#include <qopenglfunctions.h>

#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLWindow>

#include "GLCommon.hpp"

class QOpenGLShaderProgram;

class GLWindow final : public QOpenGLWindow, public GLCommon {
  Q_OBJECT

 public:
  explicit GLWindow(QWidget* parent = nullptr,
                    Qt::WindowFlags flags = Qt::WindowTransparentForInput);
  ~GLWindow();

  void Invalidate();
  inline auto getWrapperWidget() { return m_wrapperWidget; }
  void zoom(int sign);
  void requestUpdate(bool queued);
  void removeOSD(bool queued);

 protected:
  void resizeGL(int newW, int mewH) override;
  void paintGL() override;
  void initializeGL() override;
  bool event(QEvent* evt) override;

 private:
  QWidget* m_wrapperWidget;
};
