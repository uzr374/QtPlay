#include "Decoder.hpp"

#include "../Video/SupportedPixFmts.hpp"
#include "QtPlayCommon.hpp"

extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avstring.h>
#include <libavutil/bprint.h>
#include <libavutil/opt.h>
}

Decoder::Decoder() {}

Decoder::~Decoder() { destroy(); }

int configure_filtergraph(AVFilterGraph* graph, const char* filtergraph,
                          AVFilterContext* source_ctx,
                          AVFilterContext* sink_ctx) {
  int ret = 0;
  const int nb_filters = graph->nb_filters;
  AVFilterInOut *outputs = NULL, *inputs = NULL;

  if (filtergraph) {
    outputs = avfilter_inout_alloc();
    inputs = avfilter_inout_alloc();
    if (!outputs || !inputs) {
      ret = AVERROR(ENOMEM);
      goto fail;
    }

    outputs->name = av_strdup("in");
    outputs->filter_ctx = source_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = sink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    if ((ret = avfilter_graph_parse_ptr(graph, filtergraph, &inputs, &outputs,
                                        NULL)) < 0)
      goto fail;
  } else {
    if ((ret = avfilter_link(source_ctx, 0, sink_ctx, 0)) < 0) goto fail;
  }

  /* Reorder the filters to ensure that inputs of the custom filters are merged
   * first */
  for (int i = 0; i < graph->nb_filters - nb_filters; ++i)
    std::swap(graph->filters[i], graph->filters[i + nb_filters]);

  ret = avfilter_graph_config(graph, nullptr);

fail:
  avfilter_inout_free(&outputs);
  avfilter_inout_free(&inputs);
  return ret;
}

int configure_video_filters(AVFilterGraph*& graph, const Stream& video_st,
                            const char* vfilters, const AVFrame* const frame,
                            AVFilterContext*& in_video_filter,
                            AVFilterContext*& out_video_filter) {
  auto pix_fmts = supported_pix_fmts;
  char sws_flags_str[512] = {}, buffersrc_args[512] = {};
  AVFilterContext *filt_src = NULL, *filt_out = NULL, *last_filter = NULL;
  const auto fr = video_st.frameRateR(), tb = video_st.tbR(),
             sar = video_st.sarR();
  const AVDictionaryEntry* e = NULL;

  avfilter_graph_free(&graph);
  if (!(graph = avfilter_graph_alloc())) {
    return AVERROR(ENOMEM);
  }

  graph->nb_threads = 0;

  AVDictionary* sws_dict = nullptr;
  while ((e = av_dict_get(sws_dict, "", e, AV_DICT_IGNORE_SUFFIX))) {
    if (!strcmp(e->key, "sws_flags")) {
      av_strlcatf(sws_flags_str, sizeof(sws_flags_str), "%s=%s:", "flags",
                  e->value);
    } else
      av_strlcatf(sws_flags_str, sizeof(sws_flags_str), "%s=%s:", e->key,
                  e->value);
  }
  if (strlen(sws_flags_str)) sws_flags_str[strlen(sws_flags_str) - 1] = '\0';

  graph->scale_sws_opts = av_strdup(sws_flags_str);

  snprintf(buffersrc_args, sizeof(buffersrc_args),
           "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
           frame->width, frame->height, frame->format, tb.num, tb.den, sar.num,
           std::max(sar.den, 1));
  if (fr.num && fr.den)
    av_strlcatf(buffersrc_args, sizeof(buffersrc_args), ":frame_rate=%d/%d",
                fr.num, fr.den);

  int ret = 0;
  if ((ret = avfilter_graph_create_filter(
           &filt_src, avfilter_get_by_name("buffer"), "ffplay_buffer",
           buffersrc_args, NULL, graph)) < 0)
    goto fail;

  if ((ret = avfilter_graph_create_filter(
           &filt_out, avfilter_get_by_name("buffersink"), "ffplay_buffersink",
           NULL, NULL, graph)) < 0)
    goto fail;

  if ((ret = av_opt_set_int_list(filt_out, "pix_fmts", pix_fmts,
                                 AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN)) < 0)
    goto fail;

  last_filter = filt_out;

  /* Note: this macro adds a filter before the lastly added filter, so the
   * processing order of the filters is in reverse */
#define INSERT_FILT(name, arg)                                                \
  do {                                                                        \
    AVFilterContext* filt_ctx = nullptr;                                      \
                                                                              \
    ret = avfilter_graph_create_filter(&filt_ctx, avfilter_get_by_name(name), \
                                       "ffplay_" name, arg, NULL, graph);     \
    if (ret < 0) goto fail;                                                   \
                                                                              \
    ret = avfilter_link(filt_ctx, 0, last_filter, 0);                         \
    if (ret < 0) goto fail;                                                   \
                                                                              \
    last_filter = filt_ctx;                                                   \
  } while (0)

  if (true) {
    double theta = 0.0;
    const int32_t* displaymatrix = nullptr;
    const auto sd = av_frame_get_side_data(frame, AV_FRAME_DATA_DISPLAYMATRIX);
    if (sd) {
      displaymatrix = (const int32_t*)sd->data;
      if (displaymatrix) {
        theta = Stream::get_rotation(displaymatrix);
      }
    }

    if (!displaymatrix) {  // Display matrix is not present in video frame side
                           // data
      theta = video_st.rotation();
    }

    if (fabs(theta - 90) < 1.0) {
      INSERT_FILT("transpose", "clock");
    } else if (fabs(theta - 180) < 1.0) {
      INSERT_FILT("hflip", NULL);
      INSERT_FILT("vflip", NULL);
    } else if (fabs(theta - 270) < 1.0) {
      INSERT_FILT("transpose", "cclock");
    } else if (fabs(theta) > 1.0) {
      char rotate_buf[64] = {};
      snprintf(rotate_buf, sizeof(rotate_buf), "%f*PI/180", theta);
      INSERT_FILT("rotate", rotate_buf);
    }
  }

  if (frame->interlaced_frame)  // Auto-deinterlace
  {
    INSERT_FILT("yadif", nullptr);
  }

  if ((ret = configure_filtergraph(graph, vfilters, filt_src, last_filter)) < 0)
    goto fail;

  in_video_filter = filt_src;
  out_video_filter = filt_out;

fail:
  return ret;
}

int configure_audio_filters(const char* afilters, AVFilterGraph*& agraph,
                            AVFilterContext*& in_audio_filter,
                            AVFilterContext*& out_audio_filter,
                            const AudioParams& audio_tgt,
                            const AudioParams& audio_filter_src) {
  constexpr AVSampleFormat sample_fmts[] = {AV_SAMPLE_FMT_FLTP,
                                            AV_SAMPLE_FMT_NONE};
  const int sample_rates[2] = {audio_tgt.freq, -1};
  AVFilterContext *filt_asrc = NULL, *filt_asink = NULL;
  char aresample_swr_opts[512] = {}, asrc_args[512] = {};
  const AVDictionaryEntry* e = NULL;

  avfilter_graph_free(&agraph);
  if (!(agraph = avfilter_graph_alloc())) return AVERROR(ENOMEM);
  agraph->nb_threads = 1;

  AVDictionary* swr_opts = nullptr;
  while ((e = av_dict_get(swr_opts, "", e, AV_DICT_IGNORE_SUFFIX)))
    av_strlcatf(aresample_swr_opts, sizeof(aresample_swr_opts),
                "%s=%s:", e->key, e->value);
  if (strlen(aresample_swr_opts))
    aresample_swr_opts[strlen(aresample_swr_opts) - 1] = '\0';
  av_opt_set(agraph, "aresample_swr_opts", aresample_swr_opts, 0);

  AVBPrint bp = {};
  av_bprint_init(&bp, 0, AV_BPRINT_SIZE_AUTOMATIC);
  av_channel_layout_describe_bprint(audio_filter_src.ch_layout.readPtr(), &bp);

  int ret = snprintf(
      asrc_args, sizeof(asrc_args),
      "sample_rate=%d:sample_fmt=%s:time_base=%d/%d:channel_layout=%s",
      audio_filter_src.freq, av_get_sample_fmt_name(audio_filter_src.fmt), 1,
      audio_filter_src.freq, bp.str);

  ret =
      avfilter_graph_create_filter(&filt_asrc, avfilter_get_by_name("abuffer"),
                                   "ffplay_abuffer", asrc_args, NULL, agraph);
  if (ret < 0) goto end;

  ret = avfilter_graph_create_filter(&filt_asink,
                                     avfilter_get_by_name("abuffersink"),
                                     "ffplay_abuffersink", NULL, NULL, agraph);
  if (ret < 0) goto end;

  if ((ret = av_opt_set_int_list(filt_asink, "sample_fmts", sample_fmts,
                                 AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN)) <
      0)
    goto end;
  if ((ret = av_opt_set_int(filt_asink, "all_channel_counts", 1,
                            AV_OPT_SEARCH_CHILDREN)) < 0)
    goto end;
  if ((ret = av_opt_set_int(filt_asink, "all_channel_counts", 0,
                            AV_OPT_SEARCH_CHILDREN)) < 0)
    goto end;
  if ((ret = av_opt_set(filt_asink, "ch_layouts", bp.str,
                        AV_OPT_SEARCH_CHILDREN)) < 0)
    goto end;
  if ((ret = av_opt_set_int_list(filt_asink, "sample_rates", sample_rates, -1,
                                 AV_OPT_SEARCH_CHILDREN)) < 0)
    goto end;

  if ((ret = configure_filtergraph(agraph, afilters, filt_asrc, filt_asink)) <
      0)
    goto end;

  in_audio_filter = filt_asrc;
  out_audio_filter = filt_asink;

end:
  if (ret < 0) avfilter_graph_free(&agraph);
  av_bprint_finalize(&bp, NULL);

  return ret;
}

// Do all fintering job(including reconfiguring) here
void Decoder::filter_decoded_videoframe(AVFrame* frame,
                                        std::deque<Frame>& filtered_frames) {
  if (frame) {
    if (!graph || (last_w != frame->width) || (last_h != frame->height) ||
        (last_format != frame->format)) {
      if (configure_video_filters(graph, stream, nullptr, frame, filt_in,
                                  filt_out) < 0) {
        return;
      }

      last_w = frame->width;
      last_h = frame->height;
      last_format = (AVPixelFormat)frame->format;
    }
  }

  if (graph) {
    auto ret = av_buffersrc_add_frame(filt_in, frame);

    ret = 0;
    while (ret >= 0) {
      if ((ret = av_buffersink_get_frame(filt_out, filtered.av())) >= 0) {
        auto const filtered_fr = filtered.av();
        const auto frame_rate = av_buffersink_get_frame_rate(filt_out);
        const auto tb = av_buffersink_get_time_base(filt_out);
        auto& fr = filtered_frames.emplace_back();
        fr.pts = (filtered_fr->pts == AV_NOPTS_VALUE)
                     ? NAN
                     : filtered_fr->pts * av_q2d(tb);
        fr.duration = ((frame_rate.num && frame_rate.den)
                           ? av_q2d({frame_rate.den, frame_rate.num})
                           : 0.0);
        fr.pix_desc = av_pix_fmt_desc_get((AVPixelFormat)filtered_fr->format);
        av_frame_move_ref(fr.av(), filtered.av());
      }
    }
  }
};

void Decoder::filter_decoded_audioframe(AVFrame* frame,
                                        std::deque<Frame>& filtered_frames,
                                        AudioParams& audio_filter_src,
                                        const AudioParams& audio_tgt) {
  if (frame) {
    auto cmp_audio_fmts = [](AVSampleFormat fmt1, int64_t channel_count1,
                             AVSampleFormat fmt2,
                             int64_t channel_count2) -> bool {
      /* If channel count == 1, planar and non-planar formats are the same */
      if ((channel_count1 == 1LL) && (channel_count2 == 1LL))
        return (av_get_packed_sample_fmt(fmt1) !=
                av_get_packed_sample_fmt(fmt2));
      else
        return (channel_count1 != channel_count2) || (fmt1 != fmt2);
    };

    if (frame->format != AV_SAMPLE_FMT_NONE && frame->nb_samples > 0 &&
        frame->sample_rate > 0) {
      if (!graph ||
          cmp_audio_fmts(
              audio_filter_src.fmt, audio_filter_src.ch_layout.chCount(),
              (AVSampleFormat)frame->format, frame->ch_layout.nb_channels) ||
          av_channel_layout_compare(audio_filter_src.ch_layout.readPtr(),
                                    &frame->ch_layout) ||
          (audio_filter_src.freq != frame->sample_rate)) {
        // if(graph) - flush out all potentially remaining frames from the
        // filter

        audio_filter_src.ch_layout = frame->ch_layout;
        audio_filter_src.fmt = (AVSampleFormat)frame->format;
        audio_filter_src.freq = frame->sample_rate;

        if (configure_audio_filters(nullptr, graph, filt_in, filt_out,
                                    audio_tgt, audio_filter_src) < 0)
          return;
      }
    }
  }

  if (graph) {
    auto ret = av_buffersrc_add_frame(filt_in, frame);

    ret = 0;
    while (ret >= 0) {
      if ((ret = av_buffersink_get_frame(filt_out, filtered.av())) >= 0) {
        auto const filtered_fr = filtered.av();
        const auto tb = av_buffersink_get_time_base(filt_out);
        auto& fr = filtered_frames.emplace_back();
        fr.pts = (filtered_fr->pts == AV_NOPTS_VALUE)
                     ? NAN
                     : filtered_fr->pts * av_q2d(tb);
        fr.duration =
            av_q2d({filtered_fr->nb_samples, filtered_fr->sample_rate});
        av_frame_move_ref(fr.av(), filtered.av());
      }
    }
  }
};

void Decoder::destroy() {
  flush();
  stream.reset();
  next_pts = start_pts = 0;
  next_pts_tb = start_pts_tb = {};
  start_pts = AV_NOPTS_VALUE;
  isHW = false;
  use_hwdevice = false;
  use_hwframes = false;
  hw_pix_fmt = AV_PIX_FMT_NONE;
  sw_pix_fmt = AV_PIX_FMT_NONE;
  filt_in = filt_out = nullptr;
  graph = nullptr;

  if (cached_fctx) {
    av_buffer_unref(&cached_fctx);
  }

  if (avctx) {
    avcodec_free_context(&avctx);
    avctx = nullptr;
  }
}

bool Decoder::init_swdec(const Stream& st) {
  destroy();
  stream = st;

  const auto codecpar = stream.codecpar();
  const auto codec_id = codecpar->codec_id;
  const auto codec = avcodec_find_decoder(codec_id);
  if (!codec) {
    av_log(NULL, AV_LOG_WARNING, "No decoder could be found for codec %s\n",
           avcodec_get_name(codec_id));
    return false;
  }

  if (!(avctx = avcodec_alloc_context3(codec)) ||
      avcodec_parameters_to_context(avctx, codecpar) < 0)
    return false;

  avctx->pkt_timebase = stream.tbR();
  avctx->codec_id = codec->id;
  avctx->codec_type = codec->type;
  avctx->lowres = 0;
  avctx->err_recognition = 0;
  avctx->workaround_bugs = FF_BUG_AUTODETECT;

  if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
    avctx->thread_count = 0;
    avctx->thread_type = FF_THREAD_FRAME;
  } else {
    avctx->thread_count = 1;
    avctx->flags |= AV_CODEC_FLAG_BITEXACT;
    if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
      // Decoder will decode audio to this particular format if possible
      avctx->request_sample_fmt = AV_SAMPLE_FMT_FLTP;
    }
  }

  if (avcodec_open2(avctx, codec, nullptr) < 0) {
    return false;
  }

  if (stream.setStartPts()) {
    start_pts = stream.startTimeInt();
    start_pts_tb = stream.tbR();
  }

  return true;
}

AVPixelFormat get_hw_format(AVCodecContext* ctx,
                            const AVPixelFormat* pix_fmts) {
  auto dec = qtplay::ptr_cast<Decoder>(ctx->opaque);
  auto found = AV_PIX_FMT_NONE;

  for (auto p = pix_fmts; *p != AV_PIX_FMT_NONE; ++p) {
    if (*p == dec->hw_pix_fmt) {
      found = *p;
      break;
    }
  }

  if (dec->use_hwframes &&
      found != AV_PIX_FMT_NONE)  // If decoder uses AVHWFramesContext, handle it
  {
    AVBufferRef* new_fctx = nullptr;
    if (avcodec_get_hw_frames_parameters(ctx, ctx->hw_device_ctx, found,
                                         &new_fctx) < 0) {
      goto fctx_err;
    }

    if (auto const hwfctx =
            qtplay::ptr_cast<AVHWFramesContext>(new_fctx->data)) {
      if (hwfctx->initial_pool_size)  // If initial_pool_size is zero, codec
                                      // will allocate surfaces dynamically
      {
        // 1 surface is usually given by libavcodec
        if (ctx->thread_count > 1 &&
            (ctx->active_thread_type & FF_THREAD_FRAME)) {
          // If decoder is multithreaded, give 1 additional surface per thread
          hwfctx->initial_pool_size +=
              ctx->thread_count + std::max(ctx->extra_hw_frames, 0);
        }
      }

      // We actually might be able to reuse a previously allocated frame
      // context.
      if (dec->cached_fctx) {
        const auto old_fctx =
            qtplay::ptr_cast<AVHWFramesContext>(dec->cached_fctx->data);

        if (hwfctx->format != old_fctx->format ||
            hwfctx->sw_format != old_fctx->sw_format ||
            hwfctx->width != old_fctx->width ||
            hwfctx->height != old_fctx->height ||
            hwfctx->initial_pool_size != old_fctx->initial_pool_size) {
          av_buffer_unref(&dec->cached_fctx);
        }
      }
    }

    if (!dec->cached_fctx && (av_hwframe_ctx_init(new_fctx) < 0 ||
                              !(dec->cached_fctx = av_buffer_ref(new_fctx)))) {
      goto fctx_err;
    }

    ctx->hw_frames_ctx = av_buffer_ref(dec->cached_fctx);

  fctx_err:
    if (new_fctx) av_buffer_unref(&new_fctx);
  }

  return found == AV_PIX_FMT_NONE ? avcodec_default_get_format(ctx, pix_fmts)
                                  : found;
}

bool Decoder::init_hwdec(const Stream& st) {
  if (!st.isValid() || st.isAttachedPic()) return false;

  const auto codec = st.codec();
  bool success = false;
  auto type = AV_HWDEVICE_TYPE_NONE;
  while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE) {
    destroy();

    bool hwconfig_found = false;
    for (int conf_idx = 0; conf_idx <= 10000; ++conf_idx) {
      const auto codec_hwconf = avcodec_get_hw_config(codec, conf_idx);
      if (codec_hwconf) {
        const bool hwdctx_supported =
            (codec_hwconf->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX);
        const bool hwfctx_supported =
            (codec_hwconf->methods & AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX);
        if ((hwdctx_supported || hwfctx_supported) &&
            (codec_hwconf->device_type == type)) {
          use_hwframes = hwfctx_supported;
          use_hwdevice = !hwfctx_supported && hwdctx_supported;
          hw_pix_fmt = codec_hwconf->pix_fmt;
          hwconfig_found = true;
          break;
        }
      } else {
        break;
      }
    }

    if (!hwconfig_found) continue;

    if (!(avctx = avcodec_alloc_context3(codec))) return false;

    if (avcodec_parameters_to_context(avctx, st.codecpar()) < 0) continue;

    avctx->pkt_timebase = st.tbR();
    avctx->err_recognition = 0;
    avctx->workaround_bugs = FF_BUG_AUTODETECT;
    avctx->codec_id = codec->id;  // This and below may not be necessary
    avctx->codec_type = codec->type;
    avctx->lowres = 0;
    avctx->opaque = this;                     // For get_format callback
    avctx->thread_count = 1;                  // 1 additional surface
    avctx->thread_type = FF_THREAD_FRAME;     // Force 1 additional surface
    avctx->extra_hw_frames = extra_hwframes;  // Maybe too much and not
                                              // necessary
    // Allow probing hwdec for potentially unsupported formats
    avctx->hwaccel_flags |=
        (AV_HWACCEL_FLAG_ALLOW_HIGH_DEPTH |
         AV_HWACCEL_FLAG_ALLOW_PROFILE_MISMATCH | AV_HWACCEL_FLAG_IGNORE_LEVEL);
    avctx->get_format = get_hw_format;

    if (av_hwdevice_ctx_create(&avctx->hw_device_ctx, type, nullptr, nullptr,
                               0) < 0) {
      continue;
    }

    if (avcodec_open2(avctx, codec, nullptr) < 0) {
      continue;
    }

    success = true;
    break;
  }

  if (!success) {
    destroy();
    return false;
  }

  this->stream = st;
  isHW = true;

  return true;
}

bool Decoder::init(const Stream& st) {
  if (st.codecpar()->codec_type == AVMEDIA_TYPE_VIDEO && !st.isAttachedPic()) {
    if (init_hwdec(st)) {
      stream = st;
      return true;
    } else {
      destroy();
    }
  }

  const auto ok = init_swdec(st);
  if (ok) {
    stream = st;
  }

  return ok;
}

void Decoder::flush() {
  if (avctx && avcodec_is_open(avctx)) avcodec_flush_buffers(avctx);

  // By setting 'graph' to NULL we indicate that filtergraph needs to be
  // reconfigured
  if (graph) avfilter_graph_free(&graph);
  filt_in = filt_out = nullptr;

  eof_state = false;
}

int Decoder::decode_audio_packet(const Packet& apkt,
                                 std::deque<Frame>& decoded_frames,
                                 AudioParams& audio_filter_src,
                                 const AudioParams& audio_tgt) {
  auto ret =
      avcodec_send_packet(avctx, apkt.isFlush() ? nullptr : apkt.constAvData());
  ret = 0;

  while (ret >= 0) {
    ScopeManager<Frame> dm(buf_frame);
    auto const frame = buf_frame.av();
    ret = avcodec_receive_frame(avctx, frame);
    if (ret >= 0) {
      eof_state = false;

      const AVRational tb{1, frame->sample_rate};
      if (frame->pts != AV_NOPTS_VALUE)
        frame->pts = av_rescale_q(frame->pts, avctx->pkt_timebase, tb);
      else if (next_pts != AV_NOPTS_VALUE)
        frame->pts = av_rescale_q(next_pts, next_pts_tb, tb);
      if (frame->pts != AV_NOPTS_VALUE) {
        next_pts = frame->pts + frame->nb_samples;
        next_pts_tb = tb;
      }

      filter_decoded_audioframe(frame, decoded_frames, audio_filter_src,
                                audio_tgt);
    } else {
      if (ret == AVERROR_EOF) {
        filter_decoded_audioframe(nullptr, decoded_frames, audio_filter_src,
                                  audio_tgt);
        flush();
        eof_state = true;
      }
    }
  }

  return decoded_frames.size();
}

int Decoder::decode_video_packet(const Packet& vpkt,
                                 std::deque<Frame>& decoded_frames) {
  auto ret =
      avcodec_send_packet(avctx, vpkt.isFlush() ? nullptr : vpkt.constAvData());
  ret = 0;

  while (ret >= 0) {
    ret = avcodec_receive_frame(avctx, buf_frame.av());
    if (ret >= 0) {
      ScopeManager<Frame> bm(buf_frame);
      eof_state = false;

      auto frame = buf_frame.av();
      if (frame->best_effort_timestamp != AV_NOPTS_VALUE) {
        frame->pts = frame->best_effort_timestamp;
      }

      if (isHW) {
        ScopeManager<Frame> dm(downloaded_frame);
        auto const dst = downloaded_frame.av();
        if (sw_pix_fmt == AV_PIX_FMT_NONE) {
          AVPixelFormat* fmts = nullptr;
          if (av_hwframe_transfer_get_formats(
                  avctx->hw_frames_ctx, AV_HWFRAME_TRANSFER_DIRECTION_FROM,
                  &fmts, 0) >= 0) {
            if (fmts) {
              sw_pix_fmt = fmts[0];
            }
          }

          if (fmts) {
            av_freep(&fmts);
          }
        }

        const auto fctx =
            qtplay::ptr_cast<AVHWFramesContext>(avctx->hw_frames_ctx->data);
        dst->width = fctx->width;
        dst->height = fctx->height;
        dst->format = sw_pix_fmt;

        if (av_frame_get_buffer(dst, 0) < 0) {
          av_frame_unref(dst);
          break;
        }

        av_frame_make_writable(dst);

        if (av_hwframe_transfer_data(dst, frame, 0) < 0) {
          break;
        } else {
          av_frame_copy_props(dst, frame);
          dst->width = frame->width;
          dst->height = frame->height;
        }

        filter_decoded_videoframe(downloaded_frame.av(), decoded_frames);
      } else {
        filter_decoded_videoframe(buf_frame.av(), decoded_frames);
      }
    } else {
      if (ret == AVERROR_EOF) {
        filter_decoded_videoframe(nullptr, decoded_frames);
        flush();
        eof_state = true;
      }
    }
  }

  return decoded_frames.size();
}

int Decoder::decode_subs(Packet& pkt, std::deque<Subtitle>& dst_subs) {
  int ret = 0, got_sub = 0;
  if (pkt.isEOF()) {
    while (ret >= 0) {
      got_sub = 0;
      pkt.avData()->size = 0;
      pkt.avData()->data = nullptr;
      Subtitle sub;
      if ((ret = avcodec_decode_subtitle2(avctx, &sub.sub, &got_sub,
                                          pkt.avData())) >= 0 &&
          got_sub) {
        sub.m_width = avctx->width;
        sub.m_height = avctx->height;
        sub.pts = sub.sub.pts == AV_NOPTS_VALUE
                      ? 0.0
                      : (double)sub.sub.pts / AV_TIME_BASE;
        dst_subs.push_back(std::move(sub));
      } else {
        flush();
        break;
      }
    }
  } else {
    Subtitle sub;
    if (avcodec_decode_subtitle2(avctx, &sub.sub, &got_sub, pkt.avData()) >=
            0 &&
        got_sub) {
      sub.m_width = avctx->width;
      sub.m_height = avctx->height;
      sub.pts = sub.sub.pts == AV_NOPTS_VALUE
                    ? 0.0
                    : (double)sub.sub.pts / AV_TIME_BASE;
      dst_subs.push_back(std::move(sub));
    }
  }

  return 1;
}
