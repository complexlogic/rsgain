/*
 * Loudness normalizer based on the EBU R128 standard
 *
 * Copyright (c) 2014, Alessandro Ghedini
 * All rights reserved.
 *
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


#include <mutex>
#include <thread>
#include <stdlib.h>

#include <ebur128.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include <libavutil/common.h>
#include <libavutil/opt.h>
}

#define OUTPUT_FORMAT AV_SAMPLE_FMT_S16

#include "rsgain.h"
#include "scan.hpp"
#include "output.h"
#include "tag.hpp"

extern "C" {
    extern int multithread;
}

static void scan_av_log(void *avcl, int level, const char *fmt, va_list args);

static ebur128_state **scan_states     = NULL;
static enum AVCodecID *scan_codecs     = NULL;
static char          **scan_files      = NULL;
static char          **scan_containers = NULL;
static int             scan_nb_files   = 0;

#define LUFS_TO_RG(L) (-18 - L)


// FFmpeg container short names
static const char *AV_CONTAINER_NAME[] = {
    "mp3",
    "flac",
    "ogg",
    "mov,mp4,m4a,3gp,3g2,mj2",
    "asf",
    "wav",
    "wv",
    "aiff",
    "ape"
};

int name_to_id(const char *str) {
    int i;
    for (i = 0;  i < sizeof(AV_CONTAINER_NAME) / sizeof(AV_CONTAINER_NAME[0]);  i++) {
        if (!strcmp (str, AV_CONTAINER_NAME[i])) {
            return i;
        }
    }
    return -1; // error
}

int scan_init(unsigned nb_files) {
    av_log_set_callback(scan_av_log);
    scan_nb_files = nb_files;
    scan_states = (ebur128_state**) malloc(sizeof(ebur128_state *) * scan_nb_files);
    memset(scan_states, NULL, sizeof(ebur128_state*) * scan_nb_files);
    
    scan_files = (char**) malloc(sizeof(char *) * scan_nb_files);
    memset(scan_files, NULL, sizeof(char*) * scan_nb_files);

    scan_containers = (char**) malloc(sizeof(char *) * scan_nb_files);
    memset(scan_containers, NULL, sizeof(char*) * scan_nb_files);

    scan_codecs = (enum AVCodecID*) malloc(sizeof(enum AVCodecID) * scan_nb_files);
    return 0;
}

void scan_deinit() {
    int i;
    for (i = 0; i < scan_nb_files; i++) {
        if (scan_states[i] != NULL)
            ebur128_destroy(&scan_states[i]);
        free(scan_files[i]);
    free(scan_containers[i]);
    }

    free(scan_states);
}

bool scan(int nb_files, char **files, Config *config)
{
    bool error = false;
    scan_init(nb_files);
    for (int i = 0; i < nb_files && !error; i++) {
        error = scan_file(files[i], i, NULL);
    }
    if (!error)
        apply_gain(nb_files, files, config);
    scan_deinit();
    return error;
}

bool scan_file(const char *file, unsigned index, std::mutex *m) {
    int rc, stream_id = -1;
    double start = 0, len = 0;
    uint8_t *swr_out_data = NULL;
    char infotext[20];
    char infobuf[512];
    bool error = false;
    std::unique_lock<std::mutex> *lk = NULL;
    if (m != NULL)
        lk = new std::unique_lock<std::mutex>(*m, std::defer_lock);
    if (!multithread)
        output_ok("Scanning '%s' ...", file);

    // FFmpeg 5.0 workaround
    #if LIBAVCODEC_VERSION_MAJOR >= 59 
    const AVCodec *codec = NULL;
    #else
    AVCodec *codec = NULL;
    #endif

    AVPacket packet;
    AVCodecContext *ctx = NULL;
    AVFrame *frame = NULL;
    SwrContext *swr = NULL;
    AVFormatContext *container = NULL;
    ebur128_state **ebur128 = &scan_states[index];

    int buffer_size = 192000 + AV_INPUT_BUFFER_PADDING_SIZE;

    uint8_t *buffer = (uint8_t*) malloc(sizeof(uint8_t) * buffer_size);

    if (index >= scan_nb_files) {
        output_error("Index too high");
        free(buffer);
        return -1;
    }

    scan_files[index] = strdup(file);

    if (lk != NULL)
        lk->lock();
    rc = avformat_open_input(&container, file, NULL, NULL);
    if (rc < 0) {
        char errbuf[2048];
        av_strerror(rc, errbuf, 2048);
        output_fail("Could not open input: %s", errbuf);
        error = true;
        goto end;
    }
    scan_containers[index] = strdup(container->iformat->name);
    if (!multithread)
        output_ok("Container: %s [%s]", container->iformat->long_name, container->iformat->name);

    rc = avformat_find_stream_info(container, NULL);
    if (rc < 0) {
        char errbuf[2048];
        av_strerror(rc, errbuf, 2048);

        output_fail("Could not find stream info: %s", errbuf);
        error = true;
        goto end;
    }

    /* select the audio stream */
    stream_id = av_find_best_stream(container, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);

    if (stream_id < 0) {
        output_fail("Could not find audio stream");
        error = true;
        goto end;
    }
        
    /* create decoding context */
    ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        output_fail("Could not allocate audio codec context!");
        error = true;
        goto end;
    }

    avcodec_parameters_to_context(ctx, container->streams[stream_id]->codecpar);

    /* init the audio decoder */
    rc = avcodec_open2(ctx, codec, NULL);
    if (rc < 0) {
        char errbuf[2048];
        av_strerror(rc, errbuf, 2048);
        output_fail("Could not open codec: %s", errbuf);
        error = true;
        goto end;
    }

    // try to get default channel layout (they aren’t specified in .wav files)
    if (!ctx->channel_layout)
        ctx->channel_layout = av_get_default_channel_layout(ctx->channels);

    // show some information about the file
    // only show bits/sample where it makes sense
    infotext[0] = '\0';
    if (ctx->bits_per_raw_sample > 0 || ctx->bits_per_coded_sample > 0) {
        snprintf(infotext, 
            sizeof(infotext), 
            "%d bit, ",
            ctx->bits_per_raw_sample > 0 ? ctx->bits_per_raw_sample : ctx->bits_per_coded_sample
        );
    }

    av_get_channel_layout_string(infobuf, sizeof(infobuf), -1, ctx->channel_layout);
    if (!multithread)
        output_ok("Stream #%d: %s, %s%d Hz, %d ch, %s",
            stream_id, 
            codec->long_name, 
            infotext, 
            ctx->sample_rate, 
            ctx->channels, 
            infobuf
        );

    scan_codecs[index] = codec->id;
    av_init_packet(&packet);
    packet.data = buffer;
    packet.size = buffer_size;

    // Only initialize swresample if we need to convert the format
    if (ctx->sample_fmt != OUTPUT_FORMAT) {
        swr = swr_alloc();
        av_opt_set_channel_layout(swr, "in_channel_layout", ctx->channel_layout, 0);
        av_opt_set_channel_layout(swr, "out_channel_layout", ctx->channel_layout, 0);

        av_opt_set_int(swr, "in_channel_count",  ctx->channels, 0);
        av_opt_set_int(swr, "out_channel_count", ctx->channels, 0);

        av_opt_set_int(swr, "in_sample_rate", ctx->sample_rate, 0);
        av_opt_set_int(swr, "out_sample_rate", ctx->sample_rate, 0);

        av_opt_set_sample_fmt(swr, "in_sample_fmt", ctx->sample_fmt, 0);
        av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

        rc = swr_init(swr);
        if (rc < 0) {
            char errbuf[2048];
            av_strerror(rc, errbuf, 2048);
            output_fail("Could not open SWResample: %s", errbuf);
            error = true;
            goto end;
        }
    }

    *ebur128 = ebur128_init(
                   ctx->channels, 
                   ctx->sample_rate,
                   EBUR128_MODE_S | EBUR128_MODE_I | EBUR128_MODE_LRA |
                   EBUR128_MODE_SAMPLE_PEAK | EBUR128_MODE_TRUE_PEAK
                );
    if (*ebur128 == NULL) {
        output_fail("Could not initialize EBU R128 scanner");
        error = true;
        goto end;
    }

    frame = av_frame_alloc();
    if (frame == NULL) {
        output_fail("OOM");
        error = true;
        goto end;
    }

    if (container->streams[stream_id]->start_time != AV_NOPTS_VALUE)
        start = container->streams[stream_id]->start_time *
                av_q2d(container->streams[stream_id]->time_base);

    if (container->streams[stream_id]->duration != AV_NOPTS_VALUE)
        len   = container->streams[stream_id]->duration *
                av_q2d(container->streams[stream_id]->time_base);

    progress_bar(0, 0, 0, 0);
    if (lk != NULL)
        lk->unlock();
    
    while (av_read_frame(container, &packet) >= 0) {
        if (packet.stream_index == stream_id) {
            rc = avcodec_send_packet(ctx, &packet);
            if (rc < 0) {
                output_error("Error while sending a packet to the decoder");
                error = true;
                break;
            }

            while (rc >= 0) {
                rc = avcodec_receive_frame(ctx, frame);
                if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF) {
                    break;
                } 
                else if (rc < 0) {
                    output_error("Error while receiving a frame from the decoder");
                    error = true;
                    goto end;
                }
                if (rc >= 0) {
                    double pos = frame->pkt_dts*av_q2d(container->streams[stream_id]->time_base);

                    // Convert frame with swresample if necessary
                    if (swr != NULL) {
                        size_t out_size;
                        int  out_linesize;
                        out_size = av_samples_get_buffer_size(&out_linesize, 
                                        frame->channels, 
                                        frame->nb_samples, 
                                        AV_SAMPLE_FMT_S16, 
                                        0
                                   );

                        swr_out_data = (uint8_t*) av_malloc(out_size);

                        if (swr_convert(swr, 
                        (uint8_t**) &swr_out_data, 
                        frame->nb_samples,
                        (const uint8_t**) frame->data, 
                        frame->nb_samples) < 0) {
                            output_fail("Cannot convert");
                            error = true;
                            goto end;
                        }
                        rc = ebur128_add_frames_short(*ebur128, 
                                 (short *) swr_out_data, 
                                 frame->nb_samples
                             );
                    }
                    else {
                        rc = ebur128_add_frames_short(*ebur128, 
                                 (short *) frame->data[0], 
                                 frame->nb_samples
                             );
                    }
                    if (rc != EBUR128_SUCCESS)
                        output_error("Error filtering");

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
    
    if (swr != NULL) {
        swr_close(swr);
        swr_free(&swr);
        av_free(swr_out_data);
    }


    avcodec_close(ctx);
    avformat_close_input(&container);
    
    free(buffer);
    delete lk;
    return error;
}

void apply_gain(int nb_files, char *files[], Config *config)
{
    int i;
    // check for different file (codec) types in an album and warn
    // (including Opus might mess up album gain)
    if (config->do_album) {
        if (scan_album_has_different_containers() || scan_album_has_different_codecs()) {
            output_warn("You have different file types in the same album!");
            if (scan_album_has_opus())
                output_fail("Cannot calculate correct album gain when mixing Opus and non-Opus files!");
        }
    }

    if (config->tab_output)
        fputs("File\tLoudness\tRange\tTrue_Peak\tTrue_Peak_dBTP\tReference\tWill_clip\tClip_prevent\tGain\tNew_Peak\tNew_Peak_dBTP\n", stdout);

    for (i = 0; i < nb_files; i++) {
        bool will_clip = false;
        double tgain = 1.0; // "gained" track peak
        double tnew;
        double tpeak = pow(10.0, config->max_true_peak_level / 20.0); // track peak limit
        double again = 1.0; // "gained" album peak
        double anew;
        double apeak = pow(10.0, config->max_true_peak_level / 20.0); // album peak limit
        bool tclip = false;
        bool aclip = false;

        scan_result *scan = scan_get_track_result(i, config->pre_gain);

        if (scan == NULL)
            continue;

        if (config->do_album)
            scan_set_album_result(scan, config->pre_gain);

        // Check if track or album will clip, and correct if so requested (-k/-K)

        // track peak after gain
        tgain = pow(10.0, scan->track_gain / 20.0) * scan->track_peak;
        tnew = tgain;
        if (config->do_album) {
            // album peak after gain
            again = pow(10.0, scan->album_gain / 20.0) * scan->album_peak;
            anew = again;
        }

        if ((tgain > tpeak) || (config->do_album && (again > apeak)))
            will_clip = true;

        // printf("\ntrack: %.2f LU, peak %.6f; album: %.2f LU, peak %.6f\ntrack: %.6f, %.6f; album: %.6f, %.6f; Clip: %s\n",
        // 	scan->track_gain, scan->track_peak, scan->album_gain, scan->album_peak,
        // 	tgain, tpeak, again, apeak, will_clip ? "Yes" : "No");

        if (will_clip && config->no_clip) {
            if (tgain > tpeak) {
                // set new track peak = minimum of peak after gain and peak limit
                tnew = FFMIN(tgain, tpeak);
                scan->track_gain = scan->track_gain - (log10(tgain/tnew) * 20.0);
                tclip = true;
            }

            if (config->do_album && (again > apeak)) {
                anew = FFMIN(again, apeak);
                scan->album_gain = scan->album_gain - (log10(again/anew) * 20.0);
                aclip = true;
            }

            will_clip = false;

            // fprintf(stdout, "\nAfter clipping prevention:\ntrack: %.2f LU, peak %.6f; album: %.2f LU, peak %.6f\ntrack: %.6f, %.6f; album: %.6f, %.6f; Clip: %s\n",
            // 	scan->track_gain, scan->track_peak, scan->album_gain, scan->album_peak,
            // 	tgain, tpeak, again, apeak, will_clip ? "Yes" : "No");
        }

        switch (config->mode) {
            case 'c': /* check tags */
                break;

            case 'd': /* delete tags */
                switch (name_to_id(scan->container)) {

                    case AV_CONTAINER_ID_MP3:
                        if (!tag_clear_mp3(scan, config))
                            output_error("Couldn't write to: %s", scan->file);
                        break;

                    case AV_CONTAINER_ID_FLAC:
                        if (!tag_clear_flac(scan))
                            output_error("Couldn't write to: %s", scan->file);
                        break;

                    case AV_CONTAINER_ID_OGG:
                        // must separate because TagLib uses fifferent File classes
                        switch (scan->codec_id) {
                            // Opus needs special handling (different RG tags, -23 LUFS ref.)
                            case AV_CODEC_ID_OPUS:
                                if (!tag_clear_ogg_opus(scan))
                                    output_error("Couldn't write to: %s", scan->file);
                                break;

                            case AV_CODEC_ID_VORBIS:
                                if (!tag_clear_ogg_vorbis(scan))
                                    output_error("Couldn't write to: %s", scan->file);
                                break;

                            case AV_CODEC_ID_FLAC:
                                if (!tag_clear_ogg_flac(scan))
                                    output_error("Couldn't write to: %s", scan->file);
                                break;

                            case AV_CODEC_ID_SPEEX:
                                if (!tag_clear_ogg_speex(scan))
                                    output_error("Couldn't write to: %s", scan->file);
                                break;

                            default:
                                output_error("Codec 0x%x in %s container not supported",
                                    scan->codec_id, scan->container);
                                break;
                        }
                        break;

                    case AV_CONTAINER_ID_MP4:
                        if (!tag_clear_mp4(scan))
                            output_error("Couldn't write to: %s", scan->file);
                        break;

                    case AV_CONTAINER_ID_ASF:
                        if (!tag_clear_asf(scan))
                            output_error("Couldn't write to: %s", scan->file);
                        break;

                    case AV_CONTAINER_ID_WAV:
                        if (!tag_clear_wav(scan, config))
                            output_error("Couldn't write to: %s", scan->file);
                        break;

                    case AV_CONTAINER_ID_AIFF:
                        if (!tag_clear_aiff(scan, config))
                            output_error("Couldn't write to: %s", scan->file);
                        break;

                    case AV_CONTAINER_ID_WV:
                        if (!tag_clear_wavpack(scan, config))
                            output_error("Couldn't write to: %s", scan->file);
                        break;

                    case AV_CONTAINER_ID_APE:
                        if (!tag_clear_ape(scan, config))
                            output_error("Couldn't write to: %s", scan->file);
                        break;

                    default:
                        output_error("File type not supported: %s", scan->container);
                        break;
                }
                break;

            case 'i': /* ID3v2 tags */
            case 'e': /* same as 'i' plus extra tags */
            case 'l': /* same as 'e' but in LU/LUFS units (instead of 'dB')*/
                switch (name_to_id(scan->container)) {

                    case AV_CONTAINER_ID_MP3:
                        if (!tag_write_mp3(scan, config))
                            output_error("Couldn't write to: %s", scan->file);
                        break;

                    case AV_CONTAINER_ID_FLAC:
                        if (!tag_write_flac(scan, config))
                            output_error("Couldn't write to: %s", scan->file);
                        break;

                    case AV_CONTAINER_ID_OGG:
                        // must separate because TagLib uses fifferent File classes
                        switch (scan->codec_id) {
                            // Opus needs special handling (different RG tags, -23 LUFS ref.)
                            case AV_CODEC_ID_OPUS:
                                if (!tag_write_ogg_opus(scan, config))
                                    output_error("Couldn't write to: %s", scan->file);
                                break;

                            case AV_CODEC_ID_VORBIS:
                                if (!tag_write_ogg_vorbis(scan, config))
                                    output_error("Couldn't write to: %s", scan->file);
                                break;

                            case AV_CODEC_ID_FLAC:
                                if (!tag_write_ogg_flac(scan, config))
                                    output_error("Couldn't write to: %s", scan->file);
                                break;

                            case AV_CODEC_ID_SPEEX:
                                if (!tag_write_ogg_speex(scan, config))
                                    output_error("Couldn't write to: %s", scan->file);
                                break;

                            default:
                                output_error("Codec 0x%x in %s container not supported",
                                    scan->codec_id, scan->container);
                                break;
                        }
                        break;

                    case AV_CONTAINER_ID_MP4:
                        if (!tag_write_mp4(scan, config))
                            output_error("Couldn't write to: %s", scan->file);
                        break;

                    case AV_CONTAINER_ID_ASF:
                        if (!tag_write_asf(scan, config))
                            output_error("Couldn't write to: %s", scan->file);
                        break;

                    case AV_CONTAINER_ID_WAV:
                        if (!tag_write_wav(scan, config))
                            output_error("Couldn't write to: %s", scan->file);
                        break;

                    case AV_CONTAINER_ID_AIFF:
                        if (!tag_write_aiff(scan, config))
                            output_error("Couldn't write to: %s", scan->file);
                        break;

                    case AV_CONTAINER_ID_WV:
                        if (!tag_write_wavpack(scan, config))
                            output_error("Couldn't write to: %s", scan->file);
                        break;

                    case AV_CONTAINER_ID_APE:
                        if (!tag_write_ape(scan, config))
                            output_error("Couldn't write to: %s", scan->file);
                        break;

                    default:
                        output_error("File type not supported: %s", scan->container);
                        break;
                }
                break;

            case 'a': /* APEv2 tags */
                output_error("APEv2 tags are not supported");
                break;

            case 'v': /* Vorbis Comments tags */
                output_error("Vorbis Comment tags are not supported");
                break;

            case 's': /* skip tags */
                break;

            case 'r': /* force re-calculation */
                break;

            default:
                output_error("Invalid tag mode");
                break;
        }

        if (config->tab_output) {
            // output new style list: File;Loudness;Range;Gain;Reference;Peak;Peak dBTP;Clipping;Clip-prevent
            fprintf(stdout, "%s\t", scan->file);
            fprintf(stdout, "%.2f LUFS\t", scan->track_loudness);
            fprintf(stdout, "%.2f %s\t", scan->track_loudness_range, config->unit);
            fprintf(stdout, "%.6f\t", scan->track_peak);
            fprintf(stdout, "%.2f dBTP\t", 20.0 * log10(scan->track_peak));
            fprintf(stdout, "%.2f LUFS\t", scan->loudness_reference);
            fprintf(stdout, "%s\t", will_clip ? "Y" : "N");
            fprintf(stdout, "%s\t", tclip ? "Y" : "N");
            fprintf(stdout, "%.2f %s\t", scan->track_gain, config->unit);
            fprintf(stdout, "%.6f\t", tnew);
            fprintf(stdout, "%.2f dBTP\n", 20.0 * log10(tnew));

            if ((i == (nb_files - 1)) && config->do_album) {
                fprintf(stdout, "%s\t", "Album");
                fprintf(stdout, "%.2f LUFS\t", scan->album_loudness);
                fprintf(stdout, "%.2f %s\t", scan->album_loudness_range, config->unit);
                fprintf(stdout, "%.6f\t", scan->album_peak);
                fprintf(stdout, "%.2f dBTP\t", 20.0 * log10(scan->album_peak));
                fprintf(stdout, "%.2f LUFS\t", scan->loudness_reference);
                fprintf(stdout, "%s\t", (!aclip && (again > apeak)) ? "Y" : "N");
                fprintf(stdout, "%s\t", aclip ? "Y" : "N");
                fprintf(stdout, "%.2f %s\t", scan->album_gain, config->unit);
                fprintf(stdout, "%.6f\t", anew);
                fprintf(stdout, "%.2f dBTP\n", 20.0 * log10(anew));
            }
        } else {
            // output something human-readable
            output("\nTrack: %s\n", scan->file);

            output(" Loudness: %8.2f LUFS\n", scan->track_loudness);
            output(" Range:    %8.2f %s\n", scan->track_loudness_range, config->unit);
            output(" Peak:     %8.6f (%.2f dBTP)\n", scan->track_peak, 20.0 * log10(scan->track_peak));
            if (scan->codec_id == AV_CODEC_ID_OPUS) {
                // also show the Q7.8 number that goes into R128_TRACK_GAIN
                output(" Gain:     %8.2f %s (%d)%s\n", scan->track_gain, config->unit,
                 gain_to_q78num(scan->track_gain),
                 tclip ? " (corrected to prevent clipping)" : "");
            } else {
                output(" Gain:     %8.2f %s%s\n", scan->track_gain, config->unit,
                 tclip ? " (corrected to prevent clipping)" : "");
            }

            if (config->warn_clip && will_clip)
                output_error("The track will clip");

            if ((i == (nb_files - 1)) && config->do_album) {
                output("\nAlbum:\n");

                output(" Loudness: %8.2f LUFS\n", scan->album_loudness);
                output(" Range:    %8.2f %s\n", scan->album_loudness_range, config->unit);
                output(" Peak:     %8.6f (%.2f dBTP)\n", scan->album_peak, 20.0 * log10(scan->album_peak));
                if (scan->codec_id == AV_CODEC_ID_OPUS) {
                    // also show the Q7.8 number that goes into R128_ALBUM_GAIN
                    output(" Gain:     %8.2f %s (%d)%s\n", scan->album_gain, config->unit,
                    gain_to_q78num(scan->album_gain),
                        aclip ? " (corrected to prevent clipping)" : "");
                } else {
                    output(" Gain:     %8.2f %s%s\n", scan->album_gain, config->unit,
                        aclip ? " (corrected to prevent clipping)" : "");
                }
            }
            output("\n");
        }
        free(scan);
    }
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

    result = (scan_result*) malloc(sizeof(scan_result));
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

static void scan_av_log(void *avcl, int level, const char *fmt, va_list args) {

}
