#include "CSlider.hpp"

#include <QMouseEvent>

CSlider::CSlider(QWidget* parent) : QSlider(parent) {
  setMaximum(65536 * 2);
  setOrientation(Qt::Horizontal);

  connect(this, &CSlider::valueChanged, this, &CSlider::handleValChange);
}

CSlider::~CSlider() {}

bool CSlider::event(QEvent* evt) {
  const auto etype = evt->type();

  switch (etype) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:  // Move events are only emitted if at least one of
                             // the mouse buttons is being held(mouse tracking
                             // is disabled)
    {
      if (isEnabled()) {
        auto mEvt = static_cast<const QMouseEvent*>(evt);
        if (mEvt->buttons() & Qt::LeftButton) {
          setValue(((double)mEvt->x() / width()) * maximum());
        }
      }
    } break;
  }

  return QSlider::event(evt);
}

void CSlider::handleValChange(int val) {
  const double percent = std::clamp((double)val / maximum(), 0.0, 1.0);
  emit sigValChanged(percent);
}

void CSlider::setPositionPercent(double pos) {
  blockSignals(true);
  setValue(std::clamp(int(pos * maximum()), minimum(), maximum()));
  blockSignals(false);
}
