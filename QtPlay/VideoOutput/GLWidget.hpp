#pragma once

#if 1
#include <QtOpenGLWidgets/QOpenGLWidget>
#else
#include <QOpenGLWidget>
#endif

#include "GLCommon.hpp"

class GLWidget final : public QOpenGLWidget, public GLCommon {
  Q_OBJECT
 public:
  explicit GLWidget(QWidget* parent = nullptr);
  ~GLWidget();

  void Invalidate();
  void zoom(int sign);
  void sendVideoData(const Frame& frame, bool queued);
  void setOSD(const Frame& osd, bool queued);
  void requestUpdate(bool queued);
  void removeOSD(bool queued);
  Frame getCurrentFrame();

 protected:
  void resizeGL(int newW, int mewH) override;
  void paintGL() override;
  void initializeGL() override;
};
