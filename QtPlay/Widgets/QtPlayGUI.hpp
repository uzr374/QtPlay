#pragma once

#include <QtGlobal>
#include <vector>

class MainWindow;
class LoggerWidget;
class LoggerDock;
class CSlider;
class MenuBar;
class PlaylistDock;
class PlaylistWidget;
class RDFTDock;
class StatusBar;
class ToolBar;
class VideoDisplayWidget;
class VideoDock;
class WaveDock;
class VisCommon;

class QtPlayGUI final {
	Q_DISABLE_COPY_MOVE(QtPlayGUI);
	QtPlayGUI() = default;

private:
	MainWindow* mainwnd = nullptr;
	LoggerWidget* loggerw = nullptr;
	LoggerDock* loggerd = nullptr;
	MenuBar* mbar = nullptr;
	StatusBar* statbar = nullptr;
	VideoDock* vdock = nullptr;
	VideoDisplayWidget* videow = nullptr;
	WaveDock* waved = nullptr;
	VisCommon* wavew = nullptr;
	RDFTDock* rdftd = nullptr;
	VisCommon* rdftw = nullptr;
	PlaylistDock* pld = nullptr;
	PlaylistWidget* plw = nullptr;
	ToolBar* tbar = nullptr;

public:
	static void init_for(MainWindow* wnd);
	static QtPlayGUI& instance();

	VideoDisplayWidget* videoWidget();
	StatusBar* statBar();
	ToolBar* toolBar();
	PlaylistWidget* playlist();
	std::vector<VisCommon*> audioVis();
};

#define playerGUI QtPlayGUI::instance()