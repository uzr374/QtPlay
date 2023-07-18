#include "QtPlayGUI.hpp"
#include "MenuBar.hpp"
#include "MainWindow.hpp"
#include "../PlayerCore.hpp"

#include <QApplication>
#include <QDockWidget>
#include <QFileDialog>
#include <QDialog>
#include <QScreen>
#include <QSettings>
#include <QStyle>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QCloseEvent>

MainWindow::MainWindow() : QMainWindow((QWidget*)nullptr) {
  setObjectName("mainWindow");
  setWindowTitle(tr("QtPlay"));

  QtPlayGUI::init_for(this);
}

MainWindow::~MainWindow() {
  QSettings sets("Settings/MainWindow.ini", QSettings::IniFormat);
  sets.setValue("MainWindow/WindowState", saveState());
  sets.setValue("MainWindow/WindowGeom", geometry());
  sets.setValue("MainWindow/OnTop",
                bool(windowFlags() & Qt::WindowStaysOnTopHint));
}

void MainWindow::closeEvent(QCloseEvent* evt) {
    if (PlayerCore::instance().isActive()) {
        QDialog closeDialog(this);
        auto dlout = new QGridLayout;
        dlout->addWidget(new QLabel(tr("Are you sure you want to exit?")), 0, 0, Qt::AlignCenter);
        auto accButton = new QPushButton(tr("Yes")), rejButton = new QPushButton(tr("No"));
        dlout->addWidget(rejButton, 1, 0, Qt::AlignCenter);
        dlout->addWidget(accButton, 1, 1, Qt::AlignCenter);
        connect(accButton, &QPushButton::clicked, &closeDialog, &QDialog::accept);
        connect(rejButton, &QPushButton::clicked, &closeDialog, &QDialog::reject);
        closeDialog.setLayout(dlout);
        const auto res = closeDialog.exec();
        if (res == QDialog::Rejected) {
            evt->ignore();
            return;
        }
    }

    PlayerCore::instance().shutDown();
    return QMainWindow::closeEvent(evt);
}

void MainWindow::setBestWindowGeometry() {
  constexpr auto scale_fact = 0.4;
  auto curScr = screen();
  const auto scr_geom = curScr->geometry();
  const auto dstW = scr_geom.width() * scale_fact;
  const auto dstH = scr_geom.height() * scale_fact;
  const auto topLeftX = (scr_geom.width() - dstW) / 2;
  const auto topLeftY = (scr_geom.height() - dstH) / 2;
  setMinimumWidth(100);
  setMinimumHeight(100);
  const QRect recommendedGeom(topLeftX, topLeftY, dstW, dstH);
  setGeometry(recommendedGeom);
  const auto default_state = saveState();
  QSettings sets("Settings/MainWindow.ini", QSettings::IniFormat);
  const auto saved_state =
      sets.value("MainWindow/WindowState", default_state).toByteArray();
  restoreState(saved_state);

  // Hide floating docks
  for (auto w : QApplication::allWidgets()) {
    if (auto dW = qobject_cast<QDockWidget*>(w)) {
      if (dW->isFloating()) {
        dW->setFloating(false);
      }
    }
  }

  const auto saved_geom =
      sets.value("MainWindow/WindowGeom", recommendedGeom).toRect();
  if (scr_geom.contains(saved_geom)) {
    setGeometry(saved_geom);
  } else {
    setGeometry(recommendedGeom);
  }

  const auto on_top = sets.value("MainWindow/OnTop", false).toBool();
  if (on_top) {
    qobject_cast<MenuBar*>(menuBar())->alwaysOnTopAct->toggle();
  }
}

void MainWindow::handleAlwaysOnTop(bool ontop) {
  const auto wFlags = windowFlags();
  const auto on_top_bit = Qt::WindowStaysOnTopHint;
  const bool currently_on_top = (wFlags & on_top_bit);
  if (currently_on_top != ontop) {
    setWindowFlags((ontop ? (wFlags | on_top_bit) : (wFlags & ~on_top_bit)));
    show();  // setWindowFlags() hides window, so show it
  }
}

void MainWindow::triggerFileOpenDialog() {
  const auto url = QFileDialog::getOpenFileUrl(this, tr("Open file"));
  if (!url.isEmpty()) {
    const auto strUrl = url.toString();
    emit sigAddPlaylistEntry(strUrl, QString());
    emit sigOpenURL(url);
  }
}
