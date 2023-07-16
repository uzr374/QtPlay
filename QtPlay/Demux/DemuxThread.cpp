#include "DemuxThread.hpp"

#include <QtGlobal>

#include "../Audio/AudioThread.hpp"
#include "../Common/PlayerContext.hpp"
#include "../Video/VideoThread.hpp"

extern "C" {
#include <libavformat/avformat.h>
}

using qtplay::logMsg;

DemuxThread::DemuxThread(PlayerContext& _ctx) : CThread(_ctx) {}
DemuxThread::~DemuxThread() {
  abort_demuxer.store(true);
  CThread::joinOrTerminate();
}

static void stream_component_close(PlayerContext& ctx, AVFormatContext* ic,
                                   int stream_index) {
  if (stream_index < 0 || stream_index >= ic->nb_streams) return;

  const auto codecpar = ic->streams[stream_index]->codecpar;

  switch (codecpar->codec_type) {
    case AVMEDIA_TYPE_AUDIO: {
      ctx.audioq.abort();
      if (ctx.audio_thr) {
        ctx.audio_thr = nullptr;
      }

      ctx.auddec.destroy();
      ctx.audioq.flush();
      ctx.audclk.set(NAN, 0.0);
      ctx.audio_stream = -1;
    } break;
    case AVMEDIA_TYPE_VIDEO:
      ctx.videoq.abort();
      if (ctx.video_thr) {
        ctx.video_thr = nullptr;
      }

      ctx.viddec.destroy();
      ctx.videoq.flush();
      ctx.video_stream = -1;
      break;
    case AVMEDIA_TYPE_SUBTITLE: {
      std::scoped_lock slck(ctx.sub_stream_mutex);
      ctx.subtitleq.abort();
      ctx.subdec.destroy();
      ctx.subtitleq.flush();
      ctx.subtitle_stream = -1;
    } break;
    default:
      break;
  }

  ic->streams[stream_index]->discard = AVDISCARD_ALL;
}

/* open a given stream. Return 0 if OK */
static int stream_component_open(PlayerContext& ctx, AVFormatContext* ic,
                                 int stream_index) {
  if (stream_index < 0 || stream_index >= ic->nb_streams) return -1;

  int ret = 0;
  auto st = ic->streams[stream_index];
  const auto codecpar = st->codecpar;

  st->discard = AVDISCARD_DEFAULT;
  switch (codecpar->codec_type) {
    case AVMEDIA_TYPE_AUDIO: {
      ctx.last_audio_stream = stream_index;
      if (!ctx.auddec.init(Stream(ic, stream_index))) goto fail;
      ctx.audioq.start();
      ctx.audio_stream = stream_index;
      ctx.audio_thr = std::make_unique<AudioThread>(ctx);
      ctx.audio_thr->start();
    } break;
    case AVMEDIA_TYPE_VIDEO: {
      ctx.last_video_stream = stream_index;
      if (!ctx.viddec.init(Stream(ic, stream_index))) goto fail;
      ctx.videoq.start();
      ctx.video_stream = stream_index;
      ctx.video_thr = std::make_unique<VideoThread>(ctx);
      ctx.video_thr->start();
    } break;
    case AVMEDIA_TYPE_SUBTITLE: {
      std::scoped_lock lck(ctx.sub_stream_mutex);
      ctx.last_subtitle_stream = stream_index;
      if (!ctx.subdec.init(Stream(ic, stream_index))) goto fail;
      ctx.subtitleq.start();
      ctx.subtitle_stream = stream_index;
    } break;
    default:
      break;
  }

fail:

  return ret;
}

static void stream_cycle_channel(PlayerContext& ctx, AVFormatContext* ic,
                                 AVMediaType codec_type) {
  int start_index = 0, stream_index = 0, old_index = 0,
      nb_streams = ic->nb_streams;

  if (codec_type == AVMEDIA_TYPE_VIDEO) {
    start_index = ctx.last_video_stream;
    old_index = ctx.video_stream;
  } else if (codec_type == AVMEDIA_TYPE_AUDIO) {
    start_index = ctx.last_audio_stream;
    old_index = ctx.audio_stream;
  } else {
    start_index = ctx.last_subtitle_stream;
    old_index = ctx.subtitle_stream;
  }
  stream_index = start_index;

  AVProgram* p = nullptr;
  if (codec_type != AVMEDIA_TYPE_VIDEO && ctx.video_stream != -1) {
    p = av_find_program_from_stream(ic, nullptr, ctx.video_stream);
    if (p) {
      nb_streams = p->nb_stream_indexes;
      for (start_index = 0; start_index < nb_streams; start_index++)
        if (p->stream_index[start_index] == stream_index) break;
      if (start_index == nb_streams) start_index = -1;
      stream_index = start_index;
    }
  }

  for (;;) {
    if (++stream_index >= nb_streams) {
      if (codec_type == AVMEDIA_TYPE_SUBTITLE) {
        stream_index = -1;
        ctx.last_subtitle_stream = -1;
        break;
      }
      if (start_index == -1) return;
      stream_index = 0;
    }
    if (stream_index == start_index) return;

    const auto st =
        ic->streams[p ? p->stream_index[stream_index] : stream_index];
    const auto codecpar = st->codecpar;
    if (codecpar->codec_type == codec_type) {
      /* check that parameters are OK */
      if (AVMEDIA_TYPE_AUDIO == codec_type) {
        if (codecpar->sample_rate != 0 &&
            codecpar->ch_layout.nb_channels != 0) {
          break;
        }
      } else if (codec_type == AVMEDIA_TYPE_VIDEO ||
                 codec_type == AVMEDIA_TYPE_SUBTITLE) {
        break;
      }
    }
  }

  if (p && stream_index != -1) stream_index = p->stream_index[stream_index];
  logMsg("Switch %s stream from #%d to #%d\n",
         av_get_media_type_string(codec_type), old_index, stream_index);

  stream_component_close(ctx, ic, old_index);
  stream_component_open(ctx, ic, stream_index);
}

void handle_seeking(PlayerContext& ctx, AVFormatContext* ic, bool& eof_flag,
                    bool& attachments_req) {
  /* Seeking below or to the start of the stream with backwards flag set may
   * fail, so don't do that */

  std::scoped_lock sl(ctx.seek_mutex);

  const auto& seek_info = ctx.seek_info;
  if (!ctx.seek_req || seek_info.seek_type == SeekInfo::SEEK_NONE) {
    return;
  }

  if (seek_info.seek_type == SeekInfo::SEEK_STREAM_SWITCH) {
    const auto c_type = seek_info.cycle_type;
    if (c_type == AVMEDIA_TYPE_VIDEO || c_type == AVMEDIA_TYPE_AUDIO ||
        c_type == AVMEDIA_TYPE_SUBTITLE) {
      stream_cycle_channel(ctx, ic, c_type);
    } else if (seek_info.st_idx_to_open >= 0) {
    }
  } else {
    /*Don't bother to seek in an unseekable stream*/
    if ((ic->ctx_flags & AVFMTCTX_UNSEEKABLE)) {
      return;
    }
    int seek_flags = 0;
    auto stream_seek = [&](int64_t pos, int64_t rel, bool by_bytes) {
      ctx.last_seek_pos = pos;
      ctx.last_seek_rel = rel;
      if (by_bytes) seek_flags |= AVSEEK_FLAG_BYTE;
    };

    const CThread::ScopedLocker athr_l(ctx.audio_thr);
    const CThread::ScopedLocker vthr_l(ctx.video_thr);

    if (ctx.seek_info.seek_type == SeekInfo::SEEK_INCR) {
      auto incr = ctx.seek_info.incr_or_percent;
      if (ctx.seek_by_bytes > 0 && !(ic->iformat->flags & AVFMT_NO_BYTE_SEEK)) {
        long double pos = -1.0;
        if (pos < 0 && ctx.video_stream >= 0) pos = ctx.last_video_byte_pos;
        if (pos < 0 && ctx.audio_stream >= 0) pos = ctx.last_audio_byte_pos;
        if (pos < 0) pos = ic->pb ? avio_tell(ic->pb) : 0;
        if (ic->bit_rate)
          incr *= (long double)ic->bit_rate / 8.0;
        else
          incr *= 180000.0;
        pos += incr;
        stream_seek(std::max(pos, 0.0L), incr, true);
      } else {
        auto pos = ctx.best_clkval();
        if (isnan(pos)) pos = (long double)ctx.last_seek_pos / AV_TIME_BASE;
        pos += incr;
        if ((ic->start_time != AV_NOPTS_VALUE) &&
            (pos < ic->start_time / (double)AV_TIME_BASE))
          pos = ic->start_time / (double)AV_TIME_BASE;
        stream_seek(std::int64_t(pos * AV_TIME_BASE),
                    std::int64_t(incr * AV_TIME_BASE), false);
      }
    } else if (ctx.seek_info.seek_type == SeekInfo::SEEK_PERCENT) {
      const auto percent = ctx.seek_info.incr_or_percent;
      if (((ctx.seek_by_bytes > 0 || ic->duration <= 0) && ic->pb) &&
          !(ic->iformat->flags & AVFMT_NO_BYTE_SEEK)) {
        const auto size = avio_size(ic->pb);
        stream_seek(size * percent, 0, 1);
      } else {
        const int tns = ic->duration / AV_TIME_BASE;
        const int thh = tns / 3600;
        const int tmm = (tns % 3600) / 60;
        const int tss = (tns % 60);
        const int ns = int(percent * tns);
        const int hh = ns / 3600;
        const int mm = (ns % 3600) / 60;
        const int ss = (ns % 60);
        /*logMsg(
            "Seek to %2.0lf%% (%2d:%02d:%02d) of total duration "
            "(%2d:%02d:%02d)",
            percent * 100.0, hh, mm, ss, thh, tmm, tss);*/
        const auto ts =
            static_cast<std::int64_t>(percent * double(ic->duration)) +
            ((ic->start_time != AV_NOPTS_VALUE) ? ic->start_time : 0LL);
        // logMsg("Seek TS: %" PRId64, ts);
        stream_seek(ts, 0, false);
      }
    } else if (ctx.seek_info.seek_type == SeekInfo::SEEK_CHAPTER) {
      const int ch_incr = ctx.seek_info.chapter_incr;
      if (!ch_incr || !ic->nb_chapters) return;

      const std::int64_t pos = ctx.best_clkval() * AV_TIME_BASE;

#define AV_TIME_BASE_Q \
  { 1, AV_TIME_BASE }

      int i = 0;
      /* find the current chapter */
      for (i = 0; i < ic->nb_chapters; ++i) {
        const auto ch = ic->chapters[i];
        if (av_compare_ts(pos, AV_TIME_BASE_Q, ch->start, ch->time_base) < 0) {
          --i;
          break;
        }
      }

      i = std::max(i + ch_incr, 0);
      if (i >= ic->nb_chapters) return;

      logMsg("Seeking to chapter %d", i);
      stream_seek(av_rescale_q(ic->chapters[i]->start,
                               ic->chapters[i]->time_base, AV_TIME_BASE_Q),
                  0, 0);
    }

    const auto seek_pos = ctx.last_seek_pos, seek_rel = ctx.last_seek_rel,
               seek_target = seek_pos;
    // FIXME the +-2 is due to rounding being not done in the correct direction
    // in generation
    //      of the seek_pos/seek_rel variables
    const auto seek_min = seek_rel > 0 ? seek_target - seek_rel + 2 : INT64_MIN;
    const auto seek_max = seek_rel < 0 ? seek_target - seek_rel - 2 : INT64_MAX;
    const auto ret =
        avformat_seek_file(ic, -1, seek_min, seek_target, seek_max, seek_flags);
    if (ret < 0) {
      logMsg("%s: error while seeking", ic->url);
    } else {
      if (ctx.audio_stream >= 0) {
        ctx.audioq.flush();
      }

      if (ctx.video_stream >= 0) {
        ctx.videoq.flush();
      }

      if (ctx.subtitle_stream >= 0) ctx.subtitleq.flush();
    }
  }

  ctx.seek_info.reset();
  ctx.seek_req = false;
  attachments_req = true;
  eof_flag = false;
}

static bool is_realtime(AVFormatContext* s) {
  if (!strcmp(s->iformat->name, "rtp") || !strcmp(s->iformat->name, "rtsp") ||
      !strcmp(s->iformat->name, "sdp"))
    return true;

  if (s->pb && (!strncmp(s->url, "rtp:", 4) || !strncmp(s->url, "udp:", 4)))
    return true;
  return false;
}

bool demux_check_buffer_fullness(const PlayerContext& ctx,
                                 const std::vector<Stream>& streams) {
  if (ctx.audioq.isFull() || ctx.videoq.isFull() || ctx.subtitleq.isFull())
    return true;

  auto videoq_nb_packets = 0, audioq_nb_packets = 0;
  auto videoq_size = 0LL, audioq_size = 0LL, videoq_duration = 0LL,
       audioq_duration = 0LL;
  auto audio_has_enough_packets = false, video_has_enough_packets = false;

  constexpr auto MAX_QUEUE_SIZE = (50LL * 1024LL * 1024LL);
  constexpr auto MIN_FRAMES = 100;

  auto queue_is_full = [](const Stream& st, int64_t nb_pkts,
                          int64_t dur) -> bool {
    return (nb_pkts > MIN_FRAMES) && (!dur || (st.tb() * dur > 2.0));
  };

  const auto aqparams = ctx.audioq.getState(), vqparams = ctx.videoq.getState();
  const auto has_audio_st = !aqparams.abort_req,
             has_video_st = !vqparams.abort_req;
  if (has_audio_st) {
    audioq_size = std::max(0LL, aqparams.size);
    audioq_nb_packets = std::max(0, aqparams.nb_packets);
    audioq_duration = std::max(0LL, aqparams.duration);
    const auto& st = streams[ctx.audio_stream];
    audio_has_enough_packets =
        queue_is_full(st, audioq_nb_packets, audioq_duration);
  } else
    audio_has_enough_packets = true;

  if (has_video_st) {
    videoq_size = std::max(0LL, vqparams.size);
    videoq_nb_packets = std::max(0, vqparams.nb_packets);
    videoq_duration = std::max(0LL, vqparams.duration);
    const auto& st = streams[ctx.video_stream];
    video_has_enough_packets =
        st.isAttachedPic()
            ? true
            : queue_is_full(st, videoq_nb_packets, videoq_duration);
  } else
    video_has_enough_packets = true;

  return ((audioq_size + videoq_size) > MAX_QUEUE_SIZE) ||
         (video_has_enough_packets && audio_has_enough_packets);
}

void DemuxThread::run() {
  AVFormatContext* ic = nullptr;
  std::mutex wait_mutex;
  AVDictionary* format_opts = nullptr;
  auto loop = false, autoexit = false, realtime = false, last_paused = false,
       eof = false, queue_attachments_req = true, local_paused = false,
       cont = true;
  auto estimated_duration = AV_NOPTS_VALUE;
  std::vector<Stream> streams;
  Packet pkt;

  auto cleanup_func = [&] {
    stream_component_close(ctx, ic, ctx.audio_stream);
    stream_component_close(ctx, ic, ctx.video_stream);
    stream_component_close(ctx, ic, ctx.subtitle_stream);
    avformat_close_input(&ic);
  };

  ON_SCOPE_EXIT(cleanup_func, demthr_guard);

  auto wait_timeout = [&] {
    std::unique_lock lck(wait_mutex);
    ctx.continue_read_thread.wait_for(lck, std::chrono::milliseconds(10));
  };

  if (!(ic = avformat_alloc_context())) {
    logMsg("Could not allocate AVFormatContext");
    return;
  }

  ic->flags |= AVFMT_FLAG_GENPTS;

  /*Set up an interrupt callback*/
  ic->interrupt_callback.callback = [](void* ctx) -> int {
    return static_cast<int>(qtplay::ptr_cast<std::atomic_bool>(ctx)->load());
  };
  ic->interrupt_callback.opaque = std::addressof(this->abort_demuxer);

  av_dict_set(&format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
  if (avformat_open_input(&ic, ctx.filename.c_str(), nullptr, &format_opts) <
      0) {
    return;
  }

  ic->flags |= AVFMT_FLAG_GENPTS;
  av_format_inject_global_side_data(ic);
  if (avformat_find_stream_info(ic, nullptr) < 0) {
    logMsg("%s: could not find codec parameters", ctx.filename.c_str());
    return;
  }

  if (ic->duration <= 0) {
    for (int i = 0; i < ic->nb_streams; ++i) {
      const auto st = ic->streams[i];
      if (st->duration > 0 && st->duration > estimated_duration) {
        estimated_duration = st->duration;
      }
    }
  } else {
    estimated_duration = ic->duration;
  }

  ctx.stream_duration =
      (estimated_duration <= 0LL
           ? NAN
           : (long double)estimated_duration / (double)AV_TIME_BASE);

  if (ic->pb)
    ic->pb->eof_reached = 0;  // FIXME hack, ffplay maybe should not use
                              // avio_feof() to test for the end
  if (ctx.seek_by_bytes < 0)
    ctx.seek_by_bytes = !(ic->iformat->flags & AVFMT_NO_BYTE_SEEK) &&
                        !!(ic->iformat->flags & AVFMT_TS_DISCONT) &&
                        strcmp("ogg", ic->iformat->name);
  ctx.max_frame_duration =
      (ic->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;
  realtime = is_realtime(ic);

  streams.resize(ic->nb_streams);
  for (auto i = 0; i < ic->nb_streams; ++i) {
    streams.push_back(Stream(ic, i));
    ic->streams[i]->discard = AVDISCARD_ALL;
  }

  ctx.m_streams = streams;

  int video_idx = -1, audio_idx = -1, sub_idx = -1;
  for (const auto& st : streams) {
    if (st.codecpar()->codec_type == AVMEDIA_TYPE_VIDEO) {
      if (video_idx < 0 || streams[video_idx].isAttachedPic()) {
        video_idx = st.index();
      }
    } else if (st.codecpar()->codec_type == AVMEDIA_TYPE_AUDIO) {
      audio_idx = st.index();
    } else if (st.codecpar()->codec_type == AVMEDIA_TYPE_SUBTITLE) {
      sub_idx = st.index();
    }
  }

  if (audio_idx >= 0) {
    stream_component_open(ctx, ic, audio_idx);
  }

  if (video_idx >= 0) {
    stream_component_open(ctx, ic, video_idx);

    if (ctx.video_stream >= 0 && sub_idx >= 0) {
      stream_component_open(ctx, ic, sub_idx);
    }
  }

  if (!ctx.video_thr && !ctx.audio_thr) {
    av_log(NULL, AV_LOG_FATAL, "Failed to open file '%s'", ic->url);
    return;
  }

  while (cont) {
    {
      std::scoped_lock lck(thr_lock);
      if (quit_requested) {
        break;
      }

      if (is_paused != local_paused) {
        local_paused = is_paused;

        const auto res = local_paused ? av_read_pause(ic) : av_read_play(ic);

        if (ctx.audio_thr) {
          ctx.audio_thr->trySetPause(local_paused);
        }

        if (ctx.video_thr) {
          ctx.video_thr->trySetPause(local_paused);
        }

        request_received = true;
        wait_cond.notify_one();
      }
    }

    if (local_paused) {
      if (!std::strcmp(ic->iformat->name, "rtsp") ||
          (ic->pb && !std::strncmp(ic->url, "mmsh:", 5))) {
        /* wait 10 ms to avoid trying to get another packet */
        /* XXX: horrible */
        qtplay::sleep_ms(10);
        continue;
      }
    } else {
      if ((!ctx.audio_thr || ctx.audio_thr->eofReached()) &&
          (!ctx.video_thr || ctx.video_thr->eofReached())) {
          ctx.notifyEOF();
        /*if (loop) {
          ctx.request_seek(false, 0.0);
        } else if (autoexit) {
          break;
        }*/
      }
    }

    handle_seeking(ctx, ic, eof, queue_attachments_req);

    if (queue_attachments_req) {
      if ((ctx.video_stream >= 0) &&
          (ic->streams[ctx.video_stream]->disposition &
           AV_DISPOSITION_ATTACHED_PIC)) {
        if (av_packet_ref(pkt.avData(),
                          &ic->streams[ctx.video_stream]->attached_pic) >= 0) {
          ctx.videoq.put(pkt);
          ctx.videoq.put_nullpacket(ctx.video_stream, true);
          pkt.clear();
        }
      }

      queue_attachments_req = false;
    }

    /* if the queues are full, no need to read more */
    if ((ctx.video_stream >= 0 || ctx.audio_stream >= 0) &&
        demux_check_buffer_fullness(ctx, streams)) {
      /* wait 10 ms */
      wait_timeout();
    } else {
      const auto read_res = av_read_frame(ic, pkt.avData());
      if (ic->ctx_flags & AVFMTCTX_NOHEADER)  // Streams are dynamically added
      {
      }

      if (read_res < 0) {
        if (((read_res == AVERROR_EOF) || (ic->pb && avio_feof(ic->pb))) &&
            !eof) {
          eof = true;
          ctx.setDemuxerEOF(eof);
          ctx.videoq.put_nullpacket(ctx.video_stream, true);
          ctx.audioq.put_nullpacket(ctx.audio_stream, true);
          ctx.subtitleq.put_nullpacket(ctx.subtitle_stream, true);
        }

        if (ic->pb && ic->pb->error && autoexit) {
          break;
        }
        wait_timeout();
      } else {
        eof = false;
        ctx.setDemuxerEOF(eof);
        const auto pkt_st_idx = pkt.streamIndex();
        if (pkt_st_idx >= 0 &&
            pkt_st_idx < ic->nb_streams) {  // Avoid reading garbage if input is
                                            // corrupted
          if (pkt_st_idx == ctx.audio_stream) {
            ctx.audioq.put(pkt);
          } else if ((pkt_st_idx == ctx.video_stream) &&
                     !streams[pkt_st_idx].isAttachedPic()) {
            ctx.videoq.put(pkt);
          } else if (pkt_st_idx == ctx.subtitle_stream) {
            ctx.subtitleq.put(pkt);
          } else {
            pkt.clear();
          }
        } else {
          pkt.clear();
        }
      }
    }
  }
}
