#pragma once

#include <QSlider>

class CSlider final : public QSlider {
  Q_OBJECT;
  Q_DISABLE_COPY_MOVE(CSlider);

 private:
  bool event(QEvent*) override;

  Q_SLOT void handleValChange(int val);

 public:
  CSlider(QWidget* parent);
  ~CSlider();

  Q_SIGNAL void sigValChanged(double new_val);

  Q_SLOT void setPositionPercent(double pos);
};
