#include "AudioThread.hpp"

#include "../Common/PlayerContext.hpp"
#include "../Common/QtPlayCommon.hpp"
#include "../Common/QtPlaySDL.hpp"

extern "C" {
#include <libswresample/swresample.h>
}

#include <span>

using qtplay::logMsg;

/* Minimum SDL audio buffer size, in sample frames. */
constexpr auto SDL_AUDIO_MIN_BUFFER_SIZE = 512;
/* Calculate actual buffer size keeping in mind not cause too frequent audio
 * callbacks */
constexpr auto SDL_AUDIO_MAX_CALLBACKS_PER_SEC = 30;

AudioThread::AudioThread(PlayerContext& _ctx) : CThread(_ctx) {}
AudioThread::~AudioThread() { CThread::joinOrTerminate(); }

std::vector<std::float_t> resample_frame(const AVFrame* const frame,
                                         SwrContext*& swr_ctx,
                                         AudioParams& audio_src,
                                         AudioParams& audio_tgt, double& delay) {
  std::vector<std::float_t> res;
  const auto frame_nb_samples = frame->nb_samples;
  const auto frame_sample_rate = frame->sample_rate;
  const auto frame_format = (AVSampleFormat)frame->format;
  const auto wanted_nb_samples = frame_nb_samples;
  delay = 0;

  if (!swr_ctx || frame_format != audio_src.fmt ||
      av_channel_layout_compare(std::addressof(std::as_const(frame->ch_layout)),
                                audio_src.ch_layout.readPtr()) ||
      frame_sample_rate != audio_src.freq ||
      ((wanted_nb_samples != frame_nb_samples) && !swr_ctx)) {
    AVChannelLayoutRAII frame_ch_lout_copy(frame->ch_layout),
                        audio_tgt_lout(audio_tgt.ch_layout);
    const auto swr_alloc_res = swr_alloc_set_opts2(
        &swr_ctx, audio_tgt_lout.rawPtr(), audio_tgt.fmt, audio_tgt.freq,
        frame_ch_lout_copy.rawPtr(), frame_format, frame_sample_rate, 0, NULL);
    if (!swr_ctx || swr_alloc_res < 0 || swr_init(swr_ctx) < 0) {
      logMsg("Cannot create sample rate converter for conversion of %d Hz %s "
             "%d channels to %d Hz %s %d channels!",
             frame_sample_rate, av_get_sample_fmt_name(frame_format),
             frame_ch_lout_copy.chCount(), audio_tgt.freq,
             av_get_sample_fmt_name(audio_tgt.fmt),
             audio_tgt.ch_layout.chCount());
      swr_free(&swr_ctx);
      throw;
    }

    audio_src.ch_layout = AVChannelLayoutRAII(frame->ch_layout);
    audio_src.freq = frame_sample_rate;
    audio_src.fmt = frame_format;
  }

  if (swr_ctx) {
    const int out_count =
        (int64_t)wanted_nb_samples * audio_tgt.freq / frame_sample_rate + 512;
    if (wanted_nb_samples != frame_nb_samples) {
      if (swr_set_compensation(
              swr_ctx,
              (wanted_nb_samples - frame_nb_samples) * audio_tgt.freq /
                  frame_sample_rate,
              wanted_nb_samples * audio_tgt.freq / frame_sample_rate) < 0) {
        logMsg("swr_set_compensation() failed");
        throw;
      }
    }

    res.resize(std::max(out_count * audio_tgt.ch_layout.chCount(),
                        swr_get_out_samples(swr_ctx, frame_nb_samples) *
                            audio_tgt.ch_layout.chCount()));

    auto in = qtplay::ptr_cast<const uint8_t*>(frame->extended_data);
    auto out = qtplay::ptr_cast<uint8_t>(res.data());
    const auto len2 =
        swr_convert(swr_ctx, &out, out_count, in, frame_nb_samples);
    if (len2 < 0) {
      logMsg("swr_convert() failed");
      throw;
    }

    if (len2 == out_count) {
      logMsg("audio buffer is probably too small");
      if (swr_init(swr_ctx) < 0) swr_free(&swr_ctx);
    }

    res.resize(len2 * audio_tgt.ch_layout.chCount());
  }

  delay = double(swr_get_delay(swr_ctx, 1000)) / 1000.0;

  return res;
}

void audio_callback(void* opq, std::uint8_t* stream, int bytes_requested) {
  auto& ctx = *qtplay::ptr_cast<PlayerContext>(opq);
  auto& rbuf = ctx.audio_rbuf;
  const auto sample_cnt = bytes_requested / sizeof(std::float_t);
  std::span samples(qtplay::ptr_cast<std::float_t>(stream), sample_cnt);
  std::fill(samples.begin(), samples.end(), 0.0f);
  const auto to_read =
      std::min((std::size_t)rbuf.getAvailableRead(), sample_cnt);
  if (to_read > 0) {
    const auto volP = ctx.volume_percent.load(std::memory_order_relaxed);
    const auto vol = volP * volP;
    const auto muted =
        ctx.muted.load(std::memory_order_relaxed) || qFuzzyIsNull(vol);
    rbuf.read(muted ? nullptr : samples.data(), to_read);
    if (!muted && !qFuzzyCompare(vol, 1.0f)) {
      std::for_each(samples.begin(), samples.end(),
                    [vol](auto& samp) { samp *= vol; });
    }
  }
}

static int audio_open(PlayerContext& ctx,
                      const AVChannelLayout& in_channel_layout,
                      int wanted_sample_rate, AudioParams& audio_hw_params,
                      SDL_AudioDeviceID& audio_dev) {
  if (SDL_Init(SDL_INIT_AUDIO) < 0) {
    return -1;
  }

  SDL_AudioSpec wanted_spec = {}, obtained_spec = {};
  constexpr std::array<int, 8> next_nb_channels{0, 0, 1, 6, 2, 6, 4, 6};
  constexpr std::array<int, 5> next_sample_rates{0, 44100, 48000, 96000,
                                                 192000};
  int next_sample_rate_idx = next_sample_rates.size() - 1;
  AVChannelLayoutRAII in_lout_copy(in_channel_layout);
  auto const wanted_channel_layout = in_lout_copy.rawPtr();
  int wanted_nb_channels = wanted_channel_layout->nb_channels;

  if (wanted_sample_rate <= 0) {
    wanted_sample_rate = 44100;
  }

  if (const auto env = SDL_getenv("SDL_AUDIO_CHANNELS")) {
    wanted_nb_channels = atoi(env);
    av_channel_layout_uninit(wanted_channel_layout);
    av_channel_layout_default(wanted_channel_layout, wanted_nb_channels);
  }

  if (wanted_nb_channels <= 0) {
      wanted_nb_channels = 2;
  }

  if (wanted_channel_layout->order != AV_CHANNEL_ORDER_NATIVE) {
    av_channel_layout_uninit(wanted_channel_layout);
    av_channel_layout_default(wanted_channel_layout, wanted_nb_channels);
  }

  wanted_nb_channels = wanted_channel_layout->nb_channels;
  wanted_spec.channels = wanted_nb_channels;
  wanted_spec.freq = wanted_sample_rate;
  if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
    av_log(NULL, AV_LOG_ERROR, "Invalid sample rate or channel count!\n");
    return -1;
  }

  while (next_sample_rate_idx &&
         next_sample_rates[next_sample_rate_idx] >= wanted_spec.freq)
    --next_sample_rate_idx;

  wanted_spec.format = AUDIO_F32SYS;
  wanted_spec.silence = 0;
  const int buf_size = std::max(
      SDL_AUDIO_MIN_BUFFER_SIZE,
      2 << av_log2(wanted_spec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
  const auto allowed_changes = SDL_AUDIO_ALLOW_FREQUENCY_CHANGE |
      SDL_AUDIO_ALLOW_CHANNELS_CHANGE |
      SDL_AUDIO_ALLOW_SAMPLES_CHANGE;
  wanted_spec.samples = buf_size;
  wanted_spec.callback = audio_callback;
  wanted_spec.userdata = std::addressof(ctx);
  while (!(audio_dev =
      SDL_OpenAudioDevice(nullptr, 0, &wanted_spec, &obtained_spec,
          allowed_changes))) {
    logMsg("SDL_OpenAudio (%d channels, %d Hz): %s", wanted_spec.channels,
           wanted_spec.freq, SDL_GetError());
    wanted_spec.channels =
        next_nb_channels[std::min(7, (int)wanted_spec.channels)];
    if (!wanted_spec.channels) {
      wanted_spec.freq = next_sample_rates[next_sample_rate_idx--];
      wanted_spec.channels = wanted_nb_channels;
      if (!wanted_spec.freq || next_sample_rate_idx <= 0) {
        logMsg("No more combinations to try, audio open failed");
        return -1;
      }
    }
    av_channel_layout_default(wanted_channel_layout, wanted_spec.channels);
  }

  if (obtained_spec.format != AUDIO_F32SYS) {
    logMsg("SDL advised audio format %d is not supported!",
           obtained_spec.format);
    return -1;
  }

  if (obtained_spec.channels != wanted_spec.channels) {
    av_channel_layout_uninit(wanted_channel_layout);
    av_channel_layout_default(wanted_channel_layout, obtained_spec.channels);
    if (wanted_channel_layout->order != AV_CHANNEL_ORDER_NATIVE) {
      logMsg("SDL advised channel count %d is not supported!",
             obtained_spec.channels);
      return -1;
    }
  }

  audio_hw_params.fmt = AV_SAMPLE_FMT_FLT;
  audio_hw_params.freq = obtained_spec.freq;
  audio_hw_params.ch_layout = std::move(in_lout_copy);

  return (obtained_spec.size / audio_hw_params.ch_layout.chCount()) /
         sizeof(std::float_t);
}

void AudioThread::run() {
  bool reopen_audio = true, step_pending = true, local_paused = false,
       local_eof = false;
  double swr_delay = 0.0, audio_clock = 0.0;

  Packet pkt;
  std::deque<Frame> filtered_frames;
  std::vector<std::float_t> resampled_data;
  SDL_AudioDeviceID audio_dev = {};
  SwrContext* swr_ctx = nullptr;
  AudioParams audio_src, audio_filter_src, audio_tgt;
  std::int32_t audio_hw_buf_size = 0;

  auto calc_audio_rbuf_size = [](std::int32_t rate, std::int32_t chn,
                                 std::int32_t frames_per_buffer) noexcept {
    constexpr auto duration = 0.5;
    auto calculated = 0U;
    while (((double)calculated / (double)rate) / chn < duration)
      calculated += frames_per_buffer * chn;
    return calculated;
  };

  auto avis_setpause = [&](bool p) {
    for (auto vis : ctx.audio_viss) {
      if (p) {
        vis->stop();
      } else {
        vis->start();
      }
    }
  };

  auto flush_state = [&] {
    step_pending = true;
    local_eof = false;
    swr_delay = 0.0;
    ctx.last_audio_byte_pos = -1LL;
    pkt.clear();
    resampled_data.clear();
    filtered_frames.clear();
    if (swr_ctx) swr_free(&swr_ctx);
    ctx.audclk.set(NAN, 0.0);
    ctx.auddec.flush();
    if (audio_dev) SDL_LockAudioDevice(audio_dev);
    ctx.audio_rbuf.clear();
    if (audio_dev) SDL_UnlockAudioDevice(audio_dev);
    for (auto vis : ctx.audio_viss) {
      vis->clearDisplay();
    }
  };

  auto close_adev = [&audio_dev] {
    if (audio_dev) SDL_CloseAudioDevice(audio_dev);
    audio_dev = {};
    SDL_Quit();
  };

  auto cleanup_func = [&] {
    avis_setpause(true);
    flush_state();
    close_adev();
  };

  auto get_latency = [&](bool is_paused) {
    const auto hw_buf_size = static_cast<int>(is_paused) * audio_hw_buf_size;
    const auto delay =
        swr_delay +
        double(hw_buf_size * 2) / (audio_tgt.ch_layout.chCount() *
                                   audio_tgt.freq * sizeof(std::float_t)) +
        (double(ctx.audio_rbuf.getBufferedElems_Writer() +
                resampled_data.size()) /
         audio_tgt.ch_layout.chCount()) /
            audio_tgt.freq;
    return delay;
  };

  ON_SCOPE_EXIT(cleanup_func, athr_guard);

  while (true) {
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
        ctx.audclk.setPaused(local_paused);

        request_received = true;
        wait_cond.notify_one();
      }
    }

    if (reopen_audio) {
      close_adev();

      const auto codecpar = ctx.auddec.stream.codecpar();
      const auto sample_rate = codecpar->sample_rate;
      if ((audio_hw_buf_size = audio_open(ctx, codecpar->ch_layout, sample_rate,
                                          audio_tgt, audio_dev)) < 0) {
        logMsg("Failed to create audio output instance");
        break;
      }

      audio_src = audio_tgt;

      SDL_LockAudioDevice(audio_dev);
      ctx.audio_rbuf.resize(calc_audio_rbuf_size(
          audio_tgt.freq, audio_tgt.ch_layout.chCount(), audio_hw_buf_size));
      SDL_UnlockAudioDevice(audio_dev);

      logMsg(
          "Audio output created successfully. Parameters: "
          "freq=%d, channels=%d, reported buffer size=%d",
          audio_tgt.freq, audio_tgt.ch_layout.chCount(), audio_hw_buf_size);
      reopen_audio = false;
    }

    local_eof = resampled_data.empty() &&
                filtered_frames.empty() && ctx.audio_rbuf.isEmpty() &&
        ctx.audioq.isEmpty() && (ctx.auddec.eof_state || ctx.demuxerEOF());
    step_pending &= !local_eof;
    const auto paused = (local_paused || local_eof) && !step_pending;
    const auto adev_stat = SDL_GetAudioDeviceStatus(audio_dev);
    if (paused && adev_stat == SDL_AUDIO_PLAYING) {
      avis_setpause(true);
      SDL_PauseAudioDevice(audio_dev, 1);
    } else if (!paused && adev_stat != SDL_AUDIO_PLAYING) {
      avis_setpause(false);
      SDL_PauseAudioDevice(audio_dev, 0);
    }

    if (paused) {
      const auto aclk = ctx.audclk.get_nolock();
      ctx.audclk.set(
          std::isnan(audio_clock) ? aclk : audio_clock - get_latency(paused));
      qtplay::sleep_ms(10);
      continue;
    }

    if (resampled_data.empty()) {
      try {
        if (filtered_frames.empty()) {
          if (ctx.audioq.get(pkt)) {
            ctx.auddec.decode_audio_packet(pkt, filtered_frames,
                                           audio_filter_src, audio_tgt);
            pkt.clear();
          } else {
            ctx.continue_read_thread.notify_one();
          }
        }

        if (!filtered_frames.empty()) {
          const auto& fr = filtered_frames.front();
          auto const frame = fr.av();
          if (frame->format != AV_SAMPLE_FMT_NONE && frame->nb_samples > 0 &&
              frame->sample_rate > 0) {
              resampled_data =
                  resample_frame(frame, swr_ctx, audio_src, audio_tgt, swr_delay);
            if (!resampled_data.empty()) {
              if (!std::isnan(fr.pts)) {
                audio_clock = fr.pts + fr.duration;
              } else if (!std::isnan(fr.duration) && !std::isnan(audio_clock)) {
                audio_clock += fr.duration;
              } else {
                audio_clock = NAN;
              }

              if (frame->pkt_pos >= 0LL)
                ctx.last_audio_byte_pos = frame->pkt_pos;
            }
          }

          filtered_frames.pop_front();
        }
      } catch (...) {
      }
    }

    if (!resampled_data.empty()) {
      const auto to_write = std::min(
          resampled_data.size(),
          static_cast<std::size_t>(ctx.audio_rbuf.getAvailableWrite()));
      if (to_write > 0) {
        if (ctx.audio_rbuf.write(resampled_data.data(), to_write)) {
          const auto alatency = get_latency(paused);
          for (auto vis : ctx.audio_viss) {
            vis->bufferAudio(std::span(resampled_data.begin(),
                                       resampled_data.begin() + to_write),
                             alatency, audio_tgt.freq,
                             audio_tgt.ch_layout.chCount());
          }
          if (resampled_data.begin() + to_write >= resampled_data.end()) {
            resampled_data.clear();
          } else {
            auto remaining_data = decltype(resampled_data)(
                resampled_data.begin() + to_write, resampled_data.end());
            resampled_data = std::move(remaining_data);
          }

          if (!std::isnan(audio_clock)) ctx.audclk.set(audio_clock - alatency);

          step_pending = false;
        }
      } else {
        qtplay::sleep_ms(10);
      }
    }
  }
}
