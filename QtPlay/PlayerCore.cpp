#include "PlayerCore.hpp"

#include "Common/PlayerContext.hpp"
#include "Widgets/QtPlayGUI.hpp"
#include "Widgets/VideoDisplayWidget.hpp"
#include "Widgets/ToolBar.hpp"

PlayerCore::PlayerCore() {}
PlayerCore::~PlayerCore() {}

static std::unique_ptr<PlayerContext> stream_open(
    QString filename, double audio_vol, std::vector<VisCommon*> viss) {
    try {
        return std::make_unique<PlayerContext>(filename.toStdString(), audio_vol,
            viss);
    }
    catch (...) {
        return nullptr;
    }
}

bool PlayerCore::isActive() const {
    return player_inst && player_inst->read_tid;
}

static auto getVideoOutput() {
    return playerGUI.videoWidget()->getVideoOutput();
}

static void closeVideoOutput() {
    if (auto out = getVideoOutput())
        out->Invalidate();
}

static void resetControls() {
    playerGUI.toolBar()->resetPlaybackPos();
}

void PlayerCore::openURL(QUrl url) {
    shutDown();
    if (url.isValid()) {
        const auto audio_vol = playerGUI.toolBar()->getVolumePercent();
        player_inst =
            stream_open(url.isLocalFile() ? url.toLocalFile() : url.toString(),
                audio_vol, viss);
        if (!player_inst) {
            closeVideoOutput();
        }
    }
}

void PlayerCore::shutDown() {
    player_inst = nullptr;
    closeVideoOutput();
    resetControls();
}

void PlayerCore::reqSeek(double pcnt) {
    if (isActive()) player_inst->seek_by_percent(pcnt);
}

void PlayerCore::setVol(double percent) {
    const auto audio_vol = percent;
    if (player_inst) {
        player_inst->volume_percent = audio_vol;
    }
}

std::pair<double, double> PlayerCore::getPlaybackPos() {
    double pos = NAN, dur = NAN;
    if (player_inst) {
        const auto best_clock = player_inst->best_clkval();
        if (!std::isnan(best_clock)) {
            pos = best_clock;
            dur = player_inst->stream_duration.load(std::memory_order_relaxed);
        }
    }
    return { pos, dur };
}

void PlayerCore::pausePlayback() {
    if (isPlaying()) {
        player_inst->toggle_pause();
    }
}

void PlayerCore::resumePlayback() {
    if (isActive() && !isPlaying()) {
        player_inst->toggle_pause();
    }
}

void  PlayerCore::seekByIncr(double incr) {
    if (isActive()) {
        player_inst->seek_by_incr(incr);
    }
}

bool PlayerCore::isPlaying() {
    if (isActive()) {
        return player_inst->read_tid->getPauseStatus();
    }

    return false;
}

void PlayerCore::togglePause() {
    if (isActive()) {
        player_inst->toggle_pause();
    }
}

void PlayerCore::toggleMute() {
    if (isActive()) {
        player_inst->toggle_mute();
    }
}