/*
 * Loudness normalizer based on the EBU R128 standard
 *
 * Copyright (c) 2014, Alessandro Ghedini
 * All rights reserved.
 *
 * 2019-06-30 - Matthias C. Hormann
 *  calculate correct album peak
 *  TODO: This still sucks because albums are handled track-by-track.
 * 2019-08-01 - Matthias C. Hormann
 *  - Move from deprecated libavresample library to libswresample (FFmpeg)
 * 2019-08-16 - Matthias C. Hormann
 *  - Rework to use the new FFmpeg API, no more deprecated calls
 *    (needed for FFmpeg 4.2+)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>

#include <ebur128.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include <libavutil/common.h>
#include <libavutil/opt.h>

#include "scan.h"
#include "output.h"

static void scan_frame(ebur128_state *ebur128, AVFrame *frame,
                       SwrContext *swr);
static void scan_av_log(void *avcl, int level, const char *fmt, va_list args);

static ebur128_state **scan_states     = NULL;
static enum AVCodecID *scan_codecs     = NULL;
static char          **scan_files      = NULL;
static char          **scan_containers = NULL;
static int             scan_nb_files   = 0;

#define LUFS_TO_RG(L) (-18 - L)

int scan_init(unsigned nb_files) {
#ifdef LAVF_REGISTER_ALL
  /*
	 * av_register_all() got deprecated in lavf 58.9.100
	 * It is now useless
	 * https://github.com/FFmpeg/FFmpeg/blob/70d25268c21cbee5f08304da95be1f647c630c15/doc/APIchanges#L86
	 */
  if (avformat_version() < AV_VERSION_INT(58,9,100))
    av_register_all();
#endif /* LAVF_REGISTER_ALL */

	av_log_set_callback(scan_av_log);

	scan_nb_files = nb_files;

	scan_states = malloc(sizeof(ebur128_state *) * scan_nb_files);
	if (scan_states == NULL)
		output_fail("OOM");

	scan_files = malloc(sizeof(char *) * scan_nb_files);
	if (scan_files == NULL)
		output_fail("OOM");

  scan_containers = malloc(sizeof(char *) * scan_nb_files);
	if (scan_containers == NULL)
		output_fail("OOM");

	scan_codecs = malloc(sizeof(enum AVCodecID) * scan_nb_files);
	if (scan_codecs == NULL)
		output_fail("OOM");

	return 0;
}

void scan_deinit() {
	int i;

	for (i = 0; i < scan_nb_files; i++) {
		ebur128_destroy(&scan_states[i]);
		free(scan_files[i]);
    free(scan_containers[i]);
	}

	free(scan_states);
}

int scan_file(const char *file, unsigned index) {
	int rc, stream_id = -1;
	double start = 0, len = 0;
  char infotext[20];
  char infobuf[512];

	AVFormatContext *container = NULL;

	AVCodec *codec;
	AVCodecContext *ctx;

	AVFrame *frame;
	AVPacket packet;

	SwrContext *swr;

	ebur128_state **ebur128 = &scan_states[index];

	int buffer_size = 192000 + AV_INPUT_BUFFER_PADDING_SIZE;

	uint8_t *buffer = malloc(sizeof(uint8_t) * buffer_size);

	if (index >= scan_nb_files) {
		output_error("Index too high");
		free(buffer);
		return -1;
	}

	scan_files[index] = strdup(file);

	rc = avformat_open_input(&container, file, NULL, NULL);
	if (rc < 0) {
		char errbuf[2048];
		av_strerror(rc, errbuf, 2048);

		output_fail("Could not open input: %s", errbuf);
	}
  scan_containers[index] = strdup(container->iformat->name);
  output_ok("Container: %s [%s]", container->iformat->long_name, container->iformat->name);

	rc = avformat_find_stream_info(container, NULL);
	if (rc < 0) {
		char errbuf[2048];
		av_strerror(rc, errbuf, 2048);

		output_fail("Could not find stream info: %s", errbuf);
	}

  /* select the audio stream */
  stream_id = av_find_best_stream(container, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);

	if (stream_id < 0)
		output_fail("Could not find audio stream");
		

  /* create decoding context */
  ctx = avcodec_alloc_context3(codec);
  if (!ctx)
    output_fail("Could not allocate audio codec context!");

  avcodec_parameters_to_context(ctx, container->streams[stream_id]->codecpar);

  /* init the audio decoder */
	rc = avcodec_open2(ctx, codec, NULL);
	if (rc < 0) {
		char errbuf[2048];
		av_strerror(rc, errbuf, 2048);

		output_fail("Could not open codec: %s", errbuf);
	}

  // try to get default channel layout (they aren’t specified in .wav files)
  if (!ctx->channel_layout)
    ctx->channel_layout = av_get_default_channel_layout(ctx->channels);

  // show some information about the file
  // only show bits/sample where it makes sense
  infotext[0] = '\0';
  if (ctx->bits_per_raw_sample > 0 || ctx->bits_per_coded_sample > 0) {
    snprintf(infotext, sizeof(infotext), "%d bit, ",
      ctx->bits_per_raw_sample > 0 ? ctx->bits_per_raw_sample : ctx->bits_per_coded_sample);
  }
  av_get_channel_layout_string(infobuf, sizeof(infobuf), -1, ctx->channel_layout);
  output_ok("Stream #%d: %s, %s%d Hz, %d ch, %s",
    stream_id, codec->long_name, infotext, ctx->sample_rate, ctx->channels, infobuf);

	scan_codecs[index] = codec->id;

	av_init_packet(&packet);

	packet.data = buffer;
	packet.size = buffer_size;

	swr = swr_alloc();

	*ebur128 = ebur128_init(
		ctx->channels, ctx->sample_rate,
		EBUR128_MODE_S | EBUR128_MODE_I | EBUR128_MODE_LRA |
		EBUR128_MODE_SAMPLE_PEAK | EBUR128_MODE_TRUE_PEAK
	);
	if (*ebur128 == NULL)
		output_fail("Could not initialize EBU R128 scanner");

	frame = av_frame_alloc();
	if (frame == NULL)
		output_fail("OOM");

	if (container->streams[stream_id]->start_time != AV_NOPTS_VALUE)
		start = container->streams[stream_id]->start_time *
		        av_q2d(container->streams[stream_id]->time_base);

	if (container->streams[stream_id]->duration != AV_NOPTS_VALUE)
		len   = container->streams[stream_id]->duration *
		        av_q2d(container->streams[stream_id]->time_base);

	progress_bar(0, 0, 0, 0);

	while (av_read_frame(container, &packet) >= 0) {
		if (packet.stream_index == stream_id) {

      rc = avcodec_send_packet(ctx, &packet);
      if (rc < 0) {
        output_error("Error while sending a packet to the decoder");
        break;
      }

      while (rc >= 0) {
        rc = avcodec_receive_frame(ctx, frame);
        if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF) {
            break;
        } else if (rc < 0) {
            output_error("Error while receiving a frame from the decoder");
            goto end;
        }
        if (rc >= 0) {
          double pos = frame->pkt_dts *
  				             av_q2d(container->streams[stream_id]->time_base);
  				scan_frame(*ebur128, frame, swr);

          if (pos >= 0)
            progress_bar(1, pos - start, len, 0);
        }
      }

      av_frame_unref(frame);
    }

		av_packet_unref(&packet);
	}

  // complete progress bar for very short files (only cosmetic)
  progress_bar(1, len, len, 0);

end:
	progress_bar(2, 0, 0, 0);

	av_frame_free(&frame);

	swr_free(&swr);

	avcodec_close(ctx);

	avformat_close_input(&container);
	
	free(buffer);

	return 0;
}

scan_result *scan_get_track_result(unsigned index, double pre_gain) {
	unsigned ch;

	double global, range, peak = 0.0;

	scan_result *result = NULL;
	ebur128_state *ebur128 = NULL;

	if (index >= scan_nb_files) {
		output_error("Index too high");
		return NULL;
	}

	result = malloc(sizeof(scan_result));
	if (result == NULL)
		output_fail("OOM");

	ebur128 = scan_states[index];

	if (ebur128_loudness_global(ebur128, &global) != EBUR128_SUCCESS)
		global = 0.0;

	if (ebur128_loudness_range(ebur128, &range) != EBUR128_SUCCESS)
		range = 0.0;

	for (ch = 0; ch < ebur128->channels; ch++) {
		double tmp;

		if (ebur128_true_peak(ebur128, ch, &tmp) != EBUR128_SUCCESS)
			continue;

		peak = FFMAX(peak, tmp);
	}

  // Opus is always based on -23 LUFS, we have to adapt
  if (scan_codecs[index] == AV_CODEC_ID_OPUS)
    pre_gain = pre_gain - 5.0f;

	result->file                 = scan_files[index];
  result->container            = scan_containers[index];
	result->codec_id             = scan_codecs[index];

	result->track_gain           = LUFS_TO_RG(global) + pre_gain;
	result->track_peak           = peak;
	result->track_loudness       = global;
	result->track_loudness_range = range;

	result->album_gain           = 0.f;
	result->album_peak           = 0.f;
	result->album_loudness       = 0.f;
	result->album_loudness_range = 0.f;

  result->loudness_reference   = LUFS_TO_RG(-pre_gain);

	return result;
}

int scan_album_has_different_containers() {
  int i;
  for (i = 0; i < scan_nb_files; i++) {
    if (strcmp(scan_containers[0], scan_containers[i]))
      return 1; // true
  }
  return 0; // false
}

int scan_album_has_different_codecs() {
  int i;
  for (i = 0; i < scan_nb_files; i++) {
    if (scan_codecs[0] != scan_codecs[i])
      return 1; // true
  }
  return 0; // false
}

int scan_album_has_opus() {
  int i;
  for (i = 0; i < scan_nb_files; i++) {
    if (scan_codecs[i] == AV_CODEC_ID_OPUS)
      return 1;
  }
  return 0;
}

double scan_get_album_peak() {
  double peak = 0.0;
  int i;
  unsigned ch;
  ebur128_state *ebur128 = NULL;

  for (i = 0; i < scan_nb_files; i++) {
    ebur128 = scan_states[i];

    for (ch = 0; ch < ebur128->channels; ch++) {
  		double tmp;

  		if (ebur128_true_peak(ebur128, ch, &tmp) != EBUR128_SUCCESS)
  			continue;

  		peak = FFMAX(peak, tmp);
  	}
  }
  return peak;
}

void scan_set_album_result(scan_result *result, double pre_gain) {
	double global, range;

	if (ebur128_loudness_global_multiple(
		scan_states, scan_nb_files, &global
	) != EBUR128_SUCCESS)
		global = 0.0;

	if (ebur128_loudness_range_multiple(
		scan_states, scan_nb_files, &range
	) != EBUR128_SUCCESS)
		range = 0.0;

  // Opus is always based on -23 LUFS, we have to adapt
  // When we arrive here, it’s already verified that the album
  // does NOT mix Opus and non-Opus tracks,
  // so we can safely reduce the pre-gain to arrive at -23 LUFS.
  if (scan_album_has_opus())
    pre_gain = pre_gain - 5.0f;

	result->album_gain           = LUFS_TO_RG(global) + pre_gain;
	// Calculate correct album peak (v0.2.1)
	result->album_peak           = scan_get_album_peak();
	result->album_loudness       = global;
	result->album_loudness_range = range;
}

static void scan_frame(ebur128_state *ebur128, AVFrame *frame,
                       SwrContext *swr) {
	int rc;

	uint8_t            *out_data;
	size_t              out_size;
	int                 out_linesize;
	enum AVSampleFormat out_fmt = AV_SAMPLE_FMT_S16;

	av_opt_set_channel_layout(swr, "in_channel_layout", frame->channel_layout, 0);
	av_opt_set_channel_layout(swr, "out_channel_layout", frame->channel_layout, 0);

  // add channel count to properly handle .wav reading
  av_opt_set_int(swr, "in_channel_count",  frame->channels, 0);
  av_opt_set_int(swr, "out_channel_count", frame->channels, 0);

  av_opt_set_int(swr, "in_sample_rate", frame->sample_rate, 0);
  av_opt_set_int(swr, "out_sample_rate", frame->sample_rate, 0);
	av_opt_set_sample_fmt(swr, "in_sample_fmt", frame->format, 0);
	av_opt_set_sample_fmt(swr, "out_sample_fmt", out_fmt, 0);

	rc = swr_init(swr);
	if (rc < 0) {
		char errbuf[2048];
		av_strerror(rc, errbuf, 2048);

		output_fail("Could not open SWResample: %s", errbuf);
	}

	out_size = av_samples_get_buffer_size(
		&out_linesize, frame->channels, frame->nb_samples, out_fmt, 0
	);

	out_data = av_malloc(out_size);

	if (swr_convert(
		swr, (uint8_t**) &out_data, frame->nb_samples,
		(const uint8_t**) frame->data, frame->nb_samples
	) < 0)
		output_fail("Cannot convert");

	rc = ebur128_add_frames_short(
		ebur128, (short *) out_data, frame->nb_samples
	);

	if (rc != EBUR128_SUCCESS)
		output_error("Error filtering");

	swr_close(swr);
	av_free(out_data);
}

static void scan_av_log(void *avcl, int level, const char *fmt, va_list args) {

}
