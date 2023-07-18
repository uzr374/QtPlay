#pragma once

#include "../DisplayWidgetCommon.hpp"

class VideoDisplayWidget final : public DisplayWidgetCommon {
  Q_OBJECT;
  Q_DISABLE_COPY_MOVE(VideoDisplayWidget);

 public:
  VideoDisplayWidget(QWidget* parent);
  ~VideoDisplayWidget();

  class GLWindow* getVideoOutput();
};
