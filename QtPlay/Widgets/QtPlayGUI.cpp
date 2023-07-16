#include "QtPlayGUI.hpp"

#include "LoggerDock.hpp"
#include "MenuBar.hpp"
#include "Playlist.hpp"
#include "RDFTDock.hpp"
#include "StatusBar.hpp"
#include "ToolBar.hpp"
#include "VideoDisplayWidget.hpp"
#include "VideoDock.hpp"
#include "WaveDock.hpp"

bool QtPlayGUI::class_initialized = false;
static QtPlayGUI class_instance;

void QtPlayGUI::create(MainWindow* wnd) {
	QtPlayGUI::class_initialized = true;
}

QtPlayGUI& QtPlayGUI::instance() {
	if (!QtPlayGUI::class_initialized) throw;
	return class_instance;
}