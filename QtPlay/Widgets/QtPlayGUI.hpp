#pragma once

class MainWindow;
class LoggerWidget;
class LoggerDock;
class CSlider;
class MenuBar;
class Playlist;
class RDFTDock;
class StatusBar;
class ToolBar;
class VideoDisplayWidget;
class VideoDock;
class WaveDock;

class QtPlayGUI final {
private:
	static bool class_initialized;

private:
	MainWindow* mainwnd = nullptr;
	LoggerWidget* loggerw = nullptr;
	LoggerDock* loggerd = nullptr;


public:
	static void create(MainWindow* wnd);
	static QtPlayGUI& instance();
};