#include "QtPlayGUI.hpp"

#include "MainWindow.hpp"
#include "LoggerDock.hpp"
#include "MenuBar.hpp"
#include "Playlist.hpp"
#include "RDFTDock.hpp"
#include "StatusBar.hpp"
#include "ToolBar.hpp"
#include "VideoDisplayWidget.hpp"
#include "VideoDock.hpp"
#include "WaveDock.hpp"
#include "PlaybackEventFilter.hpp"
#include "../PlayerCore.hpp"

void QtPlayGUI::init_for(MainWindow* wnd) {
    auto& inst = instance();

    wnd->setCentralWidget(new QWidget);

    wnd->setWindowIcon(wnd->style()->standardIcon(QStyle::SP_TitleBarMenuButton));
    wnd->setDockNestingEnabled(true);
    wnd->setAnimated(false);
    wnd->setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks);

    auto lgrD = new LoggerDock(wnd);
    wnd->addDockWidget(Qt::RightDockWidgetArea, lgrD);
    auto logger = qobject_cast<LoggerWidget*>(lgrD->widget());

    // Set up menu bar first
    auto mBar = new MenuBar;
    wnd->setMenuBar(mBar);

    auto statBar = new StatusBar;
    wnd->setStatusBar(statBar);

    auto tBar = new ToolBar(mBar->getBasicActions());
    wnd->addToolBar(Qt::BottomToolBarArea, tBar);

    auto videoD = new VideoDock(wnd);
    wnd->addDockWidget(Qt::LeftDockWidgetArea, videoD);
    wnd->tabifyDockWidget(lgrD, videoD);

    auto videoW = qobject_cast<VideoDisplayWidget*>(videoD->widget());
    videoW->setContextMenu(mBar->getTopLevelMenus());

    auto waveD = new WaveDock(wnd);
    wnd->addDockWidget(Qt::RightDockWidgetArea, waveD);
    auto waveW = qobject_cast<VisCommon*>(waveD->widget());

    auto rdftD = new RDFTDock(wnd);
    wnd->addDockWidget(Qt::RightDockWidgetArea, rdftD);
    wnd->tabifyDockWidget(waveD, rdftD);
    auto rdftW = qobject_cast<VisCommon*>(rdftD->widget());

    auto plD = new PlaylistDock(wnd);
    wnd->addDockWidget(Qt::RightDockWidgetArea, plD);
    wnd->tabifyDockWidget(rdftD, plD);
    auto plW = qobject_cast<PlaylistWidget*>(plD->widget());

    auto plEvtFilt = new PlaybackEventFilter(wnd);
    videoW->installEventFilter(plEvtFilt);
    waveW->installEventFilter(plEvtFilt);
    rdftW->installEventFilter(plEvtFilt);

    inst.mainwnd = wnd;
    inst.loggerd = lgrD;
    inst.loggerw = logger;
    inst.mbar = mBar;
    inst.statbar = statBar;
    inst.vdock = videoD;
    inst.videow = videoW;
    inst.waved = waveD;
    inst.wavew = waveW;
    inst.rdftd = rdftD;
    inst.rdftw = rdftW;
    inst.pld = plD;
    inst.plw = plW;
    inst.tbar = tBar;

    wnd->centralWidget()->hide();  // Hide the placeholder central widget

    // Set up all the inter-widgets connections after the UI setup
    wnd->connect(mBar, &MenuBar::sigAlwaysOnTopToggled, wnd,
        &MainWindow::handleAlwaysOnTop);
    wnd->connect(mBar, &MenuBar::sigOpenFileTriggered, wnd,
        &MainWindow::triggerFileOpenDialog);
    wnd->connect(mBar, &MenuBar::sigClearPlaylist, plW, &PlaylistWidget::clearList);

    wnd->connect(tBar, &ToolBar::sigNewVol, [](double vol) {playerCore.setVol(vol); });
    wnd->connect(tBar, &ToolBar::sigReqSeek, [](double pcnt) {playerCore.reqSeek(pcnt); });

    wnd->connect(wnd, &MainWindow::sigAddPlaylistEntry, plW,
        &PlaylistWidget::addEntry);

    // Set window geometry only after UI setup is finished
    wnd->setBestWindowGeometry();
}

QtPlayGUI& QtPlayGUI::instance() {
    static QtPlayGUI class_instance;
	return class_instance;
}

VideoDisplayWidget* QtPlayGUI::videoWidget() {
    return videow;
}

ToolBar* QtPlayGUI::toolBar() {
    return tbar;
}

StatusBar* QtPlayGUI::statBar() {
    return statbar;
}

PlaylistWidget* QtPlayGUI::playlist() {
    return plw;
}

std::vector<class VisCommon*> QtPlayGUI::audioVis() {
    return { wavew, rdftw };
}