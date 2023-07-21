#include "VideoThread.hpp"

#include "../Common/PlayerContext.hpp"
#include "SupportedPixFmts.hpp"
#include "../Widgets/QtPlayGUI.hpp"
#include "../Widgets/VideoDisplayWidget.hpp"
#include "../VideoOutput/GLWindow.hpp"

using qtplay::logMsg;

VideoThread::VideoThread(PlayerContext& _ctx) : CThread(_ctx) {}
VideoThread::~VideoThread() { CThread::joinOrTerminate(); }

void VideoThread::run() {
  const auto videoWidget = QtPlayGUI::instance().videoWidget()->getVideoOutput();
  SwsContext* sub_convert_ctx = nullptr;

  Packet pkt;
  std::deque<Subtitle> subs;
  std::deque<Frame> filtered_frames;

  /* no AV sync correction is done if below the minimum AV sync threshold */
  constexpr auto AV_SYNC_THRESHOLD_MIN = 0.04;
  /* AV sync correction is done if above the maximum AV sync threshold */
  constexpr auto AV_SYNC_THRESHOLD_MAX = 0.09;
  /* If a frame duration is longer than this, it will not be duplicated to
   * compensate AV sync */
  constexpr auto AV_SYNC_FRAMEDUP_THRESHOLD = 0.1;
  /* no AV correction is done if too big error */
  constexpr auto AV_NOSYNC_THRESHOLD = 10.0;
  /* Preferred number of frames to keep in filtered_frames during playback */
  constexpr auto preferred_buffered_frames = 2U;

  const auto is_attached_pic = ctx.viddec.stream.isAttachedPic();
  auto step_pending = true, update_frame_timer = true, can_skip = true,
       last_paused = false, local_paused = false, local_eof = false;
  const auto max_frame_duration = ctx.max_frame_duration;
  auto frame_timer = 0.0, last_pts = 0.0, last_estim_duration = 0.0;

  auto vp_duration = [](double max_duration, double cur_pts, double last_pts,
                        double framerate_duration, double last_pts_duration) {
    auto dur_probably_valid = [max_duration](double duration) {
      return !std::isnan(duration) && duration > 0.0 &&
             duration <= max_duration;
    };

    const double duration = cur_pts - last_pts;
    if (dur_probably_valid(duration)) {
      return duration;
    } else if (dur_probably_valid(last_pts_duration)) {
      return last_pts_duration;
    } else if (dur_probably_valid(framerate_duration)) {
      return framerate_duration;
    } else {  // Show ASAP
      return 0.0;
    }
  };

  auto compute_target_delay = [&](double delay, bool& skip) {
    /* update delay to follow master synchronisation source */
    /* if video is slave, we try to correct big delays by duplicating or
     * deleting a frame */
    const auto diff = ctx.vidclk.get_nolock() - ctx.audclk.get();
    /* skip or repeat frame. We take into account the
       delay to compute the threshold. I still don't know
       if it is the best guess */
    if (!std::isnan(diff) && std::fabs(diff) <= max_frame_duration) {
      const auto sync_threshold = std::max(
          AV_SYNC_THRESHOLD_MIN, std::min(AV_SYNC_THRESHOLD_MAX, delay));
      if (diff <= -sync_threshold) {
        const auto remaining_delay = delay + diff;
        if (remaining_delay <= -sync_threshold) {
          skip = true;
        }
        delay = std::max(remaining_delay, 0.0);
      } else if (diff >= sync_threshold) {
        delay =
            (delay > AV_SYNC_FRAMEDUP_THRESHOLD) ? (delay + diff) : delay * 2.0;
      }
    }

    return delay;
  };

  auto flush_state = [&] {
    ctx.vidclk.set(NAN, 0.0);
    ctx.last_video_byte_pos = -1LL;
    last_pts = frame_timer = 0.0;
    filtered_frames.clear();
    subs.clear();
    pkt.clear();
    step_pending = update_frame_timer = true;
    can_skip = local_eof = false;
    ctx.viddec.flush();
    std::scoped_lock sl(ctx.sub_stream_mutex);
    if (ctx.subtitle_stream >= 0) {
      videoWidget->removeOSD(true);
      ctx.subdec.flush();
    }
  };

  auto cleanup_func = [&] {
    flush_state();
    sws_freeContext(sub_convert_ctx);
    videoWidget->setOpened(false);
    videoWidget->requestUpdate(true);
  };

  ON_SCOPE_EXIT(cleanup_func, vthr_guard);

  videoWidget->setOpened(true);

  bool cont = true;
  while (cont) {
    {
      std::scoped_lock lck(thr_lock);
      if (quit_requested) {
        request_received = true;
        wait_cond.notify_one();
        break;
      }

      if (flush_request) {
        flush_state();
        flush_request = false;
      }

      thread_eof = local_eof;

      if ((is_paused != local_paused)) {
        local_paused = is_paused;
        ctx.vidclk.setPaused(local_paused);

        request_received = true;
        wait_cond.notify_one();
      }
    }

    local_eof = filtered_frames.empty() && ctx.videoq.isEmpty() && (ctx.viddec.eof_state || is_attached_pic || ctx.demuxerEOF());
    step_pending = step_pending && !local_eof;
    const bool paused = (local_paused && !step_pending) || local_eof;

    if (paused) {
      qtplay::sleep_ms(10);
      continue;
    } else if (paused != last_paused) {
      can_skip = false;
      frame_timer += qtplay::gettime() - ctx.vidclk.last_updated;
      last_paused = paused;
    }

    if (filtered_frames.size() < preferred_buffered_frames) {
      if (ctx.videoq.get(pkt)) {
        ctx.viddec.decode_video_packet(pkt, filtered_frames);
        pkt.clear();
      } else {
        ctx.continue_read_thread.notify_one();
      }
    }

    if (!filtered_frames.empty()) {
      auto& video_frame = filtered_frames.front();
      if (is_attached_pic) {
        videoWidget->setVideoData(std::move(video_frame));
        videoWidget->requestUpdate(true);
        filtered_frames.pop_front();
        step_pending = false;
        continue;
      }

      bool force_display = false;
      auto time = qtplay::gettime();
      if (update_frame_timer) {
        frame_timer = time;
        can_skip = update_frame_timer = false;
        force_display = true;
      }

      const auto framerate_duration = video_frame.duration;
      const auto last_duration =
          force_display
              ? 0.0
              : vp_duration(max_frame_duration, video_frame.pts, last_pts,
                            framerate_duration, last_estim_duration);
      if (last_duration > 0) last_estim_duration = last_duration;

      const auto skip_threshold =
          last_duration > 0 ? last_duration : AV_SYNC_THRESHOLD_MIN;
      bool skip = false, too_late = false;
      const auto delay = compute_target_delay(last_duration, too_late);
      skip = (too_late && can_skip);
      step_pending = false;
      can_skip = true;
      const auto next_frame_time = frame_timer + delay;
      auto time_left = next_frame_time - time;
      const auto force_forward = force_display || skip;
      const auto maybe_sleep = time_left >= 0.0015 && !force_forward;
      bool display = !maybe_sleep;

      if (maybe_sleep && (filtered_frames.size() >= preferred_buffered_frames || //Check this 'if' thoroughly
                          ctx.viddec.eof_state)) {
        const auto remaining_time = time_left;
        const auto ms = int(1000 * remaining_time);
        qtplay::sleep_ms(ms);
        time = qtplay::gettime();
        time_left = (next_frame_time - time);
        display = (time_left < 0.0015);
      }

      if (display) {
        frame_timer = next_frame_time;
        last_pts = video_frame.pts;
        if (video_frame.bytePos() >= 0LL)
          ctx.last_video_byte_pos = video_frame.bytePos();
        if (!std::isnan(last_pts)) ctx.vidclk.set(last_pts, time);

        if (delay > 0.0 &&
            -time_left > AV_SYNC_THRESHOLD_MAX)  // frame_timer is too far off
          frame_timer = time;
        else if (time_left <= -skip_threshold)
          skip = can_skip;

        {
          std::unique_lock slck(ctx.sub_stream_mutex);
          if (ctx.subtitle_stream >= 0) {
            while (subs.size() < 2 && ctx.subtitleq.get(pkt)) {
              ctx.subdec.decode_subs(pkt, subs);
              pkt.clear();
            }
          }
        }

        while (!subs.empty()) {
          auto& sp = subs.front();
          Subtitle* sp2 = nullptr;

          if (subs.size() > 1) sp2 = std::addressof(subs.at(1));

          if ((last_pts >
               (sp.pts + ((double)sp.sub.end_display_time / 1000.0))) ||
              (sp2 &&
               last_pts > (sp2->pts +
                           ((double)sp2->sub.start_display_time / 1000.0)))) {
            videoWidget->removeOSD(true);
            subs.pop_front();
          } else {
            if (!subs.empty()) {
              auto& sp = subs.front();
              if (last_pts >=
                  sp.pts + ((double)sp.sub.start_display_time / 1000.0)) {
                if (!sp.uploaded) {
                  if (!sp.m_width || !sp.m_height) {
                    sp.m_width = video_frame.width();
                    sp.m_height = video_frame.height();
                  }

                  if (!sp.m_frame.allocate(sp.m_width, sp.m_height,
                                           AV_PIX_FMT_BGRA, 1))
                    throw AVERROR(ENOMEM);

                  std::memset(sp.m_frame.data(0), 0,
                              sp.m_frame.linesize(0) * sp.m_frame.height());
                  sp.m_frame.pix_desc = av_pix_fmt_desc_get(AV_PIX_FMT_BGRA);

                  bool display_sub = false;
                  for (decltype(sp.sub.num_rects) i = 0; i < sp.sub.num_rects;
                       ++i) {
                    auto sub_rect = sp.sub.rects[i];
                    switch (sub_rect->type) {
                      case SUBTITLE_BITMAP: {
                        sub_rect->x = av_clip(sub_rect->x, 0, sp.m_width);
                        sub_rect->y = av_clip(sub_rect->y, 0, sp.m_height);
                        sub_rect->w =
                            av_clip(sub_rect->w, 0, sp.m_width - sub_rect->x);
                        sub_rect->h =
                            av_clip(sub_rect->h, 0, sp.m_height - sub_rect->y);

                        if (!(sub_convert_ctx = sws_getCachedContext(
                                  sub_convert_ctx, sub_rect->w, sub_rect->h,
                                  AV_PIX_FMT_PAL8, sub_rect->w, sub_rect->h,
                                  AV_PIX_FMT_BGRA, 0, NULL, NULL, NULL))) {
                          logMsg("Cannot initialize the conversion context");
                        } else {
                          const size_t pic_linesize = sp.m_frame.linesize(0);
                          const size_t byte_offset =
                              pic_linesize * std::max((sub_rect->y - 1), 0) +
                              sub_rect->x * 4;
                          uint8_t* pixels[4] = {};
                          int pitch[4] = {};
                          pitch[0] = pic_linesize;
                          pixels[0] = sp.m_frame.data(0) + byte_offset;
                          const auto scaleRes =
                              sws_scale(sub_convert_ctx, sub_rect->data,
                                        sub_rect->linesize, 0, sub_rect->h,
                                        pixels, pitch);
                          display_sub = true;
                        }
                      } break;
                      case SUBTITLE_ASS: {
                        logMsg((sub_rect->ass ? sub_rect->ass
                                              : "Warning: subtitle rect does "
                                                "not contain any ass line"));
                      } break;
                      case SUBTITLE_TEXT: {
                        logMsg((sub_rect->text ? sub_rect->text
                                               : "Warning: subtitle rect does "
                                                 "not contain any text"));
                      } break;
                      default:
                        break;
                    }
                  }

                  if (display_sub) {
                    videoWidget->setOSDImage(sp.m_frame);
                    videoWidget->requestUpdate(true);
                  }

                  sp.uploaded = true;
                }
              }
            }

            break;
          }
        }

        if (!skip) {
          videoWidget->setVideoData(std::move(video_frame));
          videoWidget->requestUpdate(true);
        }

        filtered_frames.pop_front();

        if (filtered_frames.size() > 0) {
          const auto& next_fr = filtered_frames.front();
          const auto dur =
              vp_duration(max_frame_duration, next_fr.pts, last_pts,
                          next_fr.duration, last_estim_duration);
          if (qtplay::gettime() >=
              frame_timer + dur)  // TODO: check for frame_timer validity
            filtered_frames.pop_front();
        }
      }
    }
  }
}
