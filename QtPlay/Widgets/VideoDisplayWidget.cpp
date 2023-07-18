#include "VideoDisplayWidget.hpp"

#include "../VideoOutput/GLWindow.hpp"

#include <QGridLayout>
#include <QResizeEvent>

static GLWindow* videoWidget = nullptr;

VideoDisplayWidget::VideoDisplayWidget(QWidget* parent)
    : DisplayWidgetCommon(parent) {
  setObjectName("videoDisplayWidget");
  setWindowTitle(tr("Video"));

  auto gLayout = new QGridLayout(this);
  gLayout->setContentsMargins(0, 0, 0, 0);
  videoWidget = new GLWindow(this);
  auto videoContainter = videoWidget->getWrapperWidget();
  videoContainter->installEventFilter(this);
  gLayout->addWidget(videoContainter);
}

VideoDisplayWidget::~VideoDisplayWidget() { }

GLWindow* VideoDisplayWidget::getVideoOutput() {
    return videoWidget;
}
