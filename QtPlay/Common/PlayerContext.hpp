#pragma once

#include "../AVWrappers/Frame.hpp"
#include "../AVWrappers/Packet.hpp"
#include "../AVWrappers/Stream.hpp"
#include "../AVWrappers/Subtitle.hpp"
#include "../Audio/AudioBuffer.hpp"
#include "../Demux/SeekInfo.hpp"
#include "../VideoOutput/GLWindow.hpp"
#include "../Visualizations/VisCommon.hpp"
#include "../Widgets/LoggerWidget.hpp"
#include "AudioParams.hpp"
#include "CThread.hpp"
#include "Clock.hpp"
#include "Decoder.hpp"
#include "PacketQueue.hpp"
#include "QtPlayCommon.hpp"

#include <atomic>
#include <memory>

struct PlayerContext final {
  Q_DISABLE_COPY_MOVE(PlayerContext);
  PlayerContext() = delete;

  std::unique_ptr<CThread> read_tid = nullptr;
  std::vector<Stream> m_streams;
  std::string filename;
  int last_video_stream = -1, last_audio_stream = -1, last_subtitle_stream = -1;
  std::condition_variable continue_read_thread;

  // Seeking
  std::mutex seek_mutex;
  bool seek_req = false;
  SeekInfo seek_info;
  int64_t last_seek_pos = 0LL, last_seek_rel = 0LL;
  std::atomic<double> stream_duration = NAN;
  int seek_by_bytes = -1;
  std::atomic_bool demuxer_eof = false;

  Clock audclk;
  Clock vidclk;

  Decoder auddec;
  Decoder viddec;
  Decoder subdec;

  std::mutex sub_stream_mutex;
  std::unique_ptr<CThread> video_thr = nullptr;

  int audio_stream = -1;
  int64_t last_audio_byte_pos = -1LL;

  PacketQueue audioq;
  std::unique_ptr<CThread> audio_thr = nullptr;
  AudioRingBufferF audio_rbuf;
  std::atomic_bool muted = false;
  std::atomic<std::float_t> volume_percent = 1.0f;
  std::vector<VisCommon*> audio_viss;

  int subtitle_stream = -1;
  PacketQueue subtitleq;

  int64_t last_video_byte_pos = -1LL;
  int video_stream = -1;
  PacketQueue videoq;
  double max_frame_duration =
      0.0;  // maximum duration of a frame - above this, we consider the jump a
            // timestamp discontinuity

  explicit PlayerContext(const std::string& url, std::float_t audio_volume,
                std::vector<VisCommon*> aviss);
  ~PlayerContext();

  double best_clkval() const;
  void request_seek(bool by_incr, double val);
  void request_stream_cycle(AVMediaType type);
  void seek_by_incr(double incr);
  void seek_by_percent(double percent);
  void toggle_pause();
  void toggle_mute();

  void notifyEOF();
  bool demuxerEOF() const { return demuxer_eof.load(std::memory_order_acquire); }
  void setDemuxerEOF(bool eof) { demuxer_eof.store(eof, std::memory_order_release); }
};
