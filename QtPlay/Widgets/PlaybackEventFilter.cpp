#include "PlaybackEventFilter.hpp"
#include "../PlayerCore.hpp"

#include <QKeyEvent>

PlaybackEventFilter::PlaybackEventFilter(QObject* parent) : QObject(parent) {}
PlaybackEventFilter::~PlaybackEventFilter() {}

bool PlaybackEventFilter::eventFilter(QObject* watched, QEvent* evt) {
	if (evt->type() == QEvent::KeyPress) {
		auto keyEvt = static_cast<const QKeyEvent*>(evt);
		const auto key = keyEvt->key();
        if (playerCore.isActive()) {
            if (key == Qt::Key_Space) {
                playerCore.togglePause();
            }
            else if (key == Qt::Key_M) {
                playerCore.toggleMute();
            }
            else if (key == Qt::Key_Right) {
                playerCore.seekByIncr(5.0);
            }
            else if (key == Qt::Key_Left) {
                playerCore.seekByIncr(-5.0);
            }
            else if (key == Qt::Key_A) {
               // player_inst->request_stream_cycle(AVMEDIA_TYPE_AUDIO);
            }
        }
	}

    return QObject::eventFilter(watched, evt);
}