#include <QApplication>
#include <QSurfaceFormat>
#include <QThread>

#include "../Widgets/MainWindow.hpp"

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
}

#include "QtPlaySDL.hpp"

// #pragma comment(lib, "portaudio.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "avfilter.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib") 
#pragma comment(lib, "swresample.lib")

void setDefaultSurfaceFormat() {
  auto fmt = QSurfaceFormat::defaultFormat();
  fmt.setSwapInterval(1);
  fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
  QSurfaceFormat::setDefaultFormat(fmt);
}

void QApplicationPreinit() {
  QApplication::setApplicationName(QApplication::tr("QtPlay"));
  QApplication::setApplicationDisplayName(QApplication::tr("QtPlay"));
  QApplication::setApplicationVersion("none");
  QApplication::setOrganizationName("DummyOrganization");
  QApplication::setOrganizationDomain("DummyDomain");
}

void ffmpeg_init() {
  av_log_set_level(AV_LOG_QUIET);
  av_log_set_callback(nullptr);
  avformat_network_init();
  avdevice_register_all();
}

void ffmpeg_deinit() { avformat_network_deinit(); }

int qtplay_exec(int argc, char** argv) {
  setDefaultSurfaceFormat();
  QApplicationPreinit();
  QApplication app(argc, argv);
  MainWindow mw;
  mw.show();
  return app.exec();
}

int main(int argc, char* argv[]) {
  SDL_SetMainReady();  // So SDL_Init() will work correctly
  ffmpeg_init();
  const auto execRes = qtplay_exec(argc, argv);
  ffmpeg_deinit();
  SDL_Quit();

  return execRes;
}
