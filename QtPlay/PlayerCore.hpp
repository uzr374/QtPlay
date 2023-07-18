#pragma once

#include <QtGlobal>
#include<QUrl>

#include <memory>
#include <vector>

class PlayerCore final {
	Q_DISABLE_COPY_MOVE(PlayerCore);
	PlayerCore();
private:
	std::unique_ptr<class PlayerContext> player_inst = nullptr;
	std::vector<class VisCommon*> viss;

public:
	~PlayerCore();

	static PlayerCore& instance() {
		static PlayerCore inst;
		return inst;
	}

	void openURL(QUrl url);
	void shutDown();
	void reqSeek(double pcnt);
	void seekByIncr(double incr);
	void setVol(double pcnt);
	void togglePause();
	void toggleMute();
	void pausePlayback();
	void resumePlayback();
	bool isPlaying();
	bool isActive() const;
	std::pair<double, double> getPlaybackPos();
};

#define playerCore PlayerCore::instance()