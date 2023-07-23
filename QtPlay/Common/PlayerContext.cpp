#include "PlayerContext.hpp"

#include "../Demux/DemuxThread.hpp"
#include "../Widgets/QtPlayGUI.hpp"
#include "../Widgets/Playlist.hpp"

PlayerContext::PlayerContext(const std::string& url, std::float_t audio_volume,
                             std::vector<VisCommon*> aviss)
    : filename(url), volume_percent(audio_volume), audio_viss(aviss) {
  (read_tid = std::make_unique<DemuxThread>(*this))->start(false);
}

PlayerContext::~PlayerContext() { read_tid = nullptr; }

double PlayerContext::best_clkval() const {
  const auto vidclk_val = vidclk.get();
  const auto audclk_val = audclk.get();
  const auto has_audioclk = !std::isnan(audclk_val),
             has_videoclk = !std::isnan(vidclk_val);
  if (has_videoclk && has_audioclk) return std::max(vidclk_val, audclk_val);
  if (has_audioclk) return audclk_val;
  return vidclk_val;
}

void PlayerContext::request_seek(bool by_incr, double val) {
  std::unique_lock lck(seek_mutex, std::try_to_lock);
  if (lck.owns_lock()) {
    seek_info.set_seek(by_incr ? SeekInfo::SEEK_INCR : SeekInfo::SEEK_PERCENT,
                       val);
    seek_req = true;
  }
}

void PlayerContext::request_stream_cycle(AVMediaType type) {
  std::unique_lock lck(seek_mutex, std::try_to_lock);
  if (lck.owns_lock()) {
    seek_info.set_stream_switch(type, -1);
    seek_req = true;
  }
}

void PlayerContext::toggle_pause() {
  if (read_tid && read_tid->isRunning()) {
    read_tid->trySetPause(!read_tid->getPauseStatus());
  }
}

void PlayerContext::toggle_mute() { muted = !muted; }

void PlayerContext::seek_by_incr(double incr) {
  if (std::fabs(incr) < 0.1)  // Avoid meaningless seeking
    return;

  request_seek(true, incr);
}

void PlayerContext::seek_by_percent(double percent) {
  request_seek(false, percent);
}

void PlayerContext::notifyEOF(){ 
    playerGUI.playlist()->playNext();
}