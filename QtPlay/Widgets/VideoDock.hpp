#pragma once

#include <QDockWidget>

#include "VideoDisplayWidget.hpp"

class VideoDock final : public QDockWidget {
  Q_OBJECT;
  Q_DISABLE_COPY_MOVE(VideoDock);

 private:
  VideoDisplayWidget* vW = nullptr;

 public:
  VideoDock(QWidget* parent);
  ~VideoDock();
};
