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
#include <set>
#include <algorithm>
#include <filesystem>
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

#include "rsgain.hpp"
#include "scan.hpp"
#include "output.hpp"
#include "tag.hpp"

extern bool multithread;

static void scan_av_log(void *avcl, int level, const char *fmt, va_list args);

static const struct extension_type extensions[] {
    {".mp2",  MP2},
    {".mp3",  MP3},
    {".flac", FLAC},
    {".ogg",  OGG},
    {".oga",  OGG},
    {".spx",  OGG},
    {".opus", OPUS},
    {".m4a",  M4A},
    {".wma",  WMA},
    {".wav",  WAV},
    {".aiff", AIFF},
    {".aif",  AIFF},
    {".snd",  AIFF},
    {".wv",   WAVPACK},
    {".ape",  APE}
};

// A function to determine a file type
inline static FileType determine_filetype(const std::string &extension)
{
    auto it = std::find_if(std::cbegin(extensions), 
                  std::cend(extensions), 
                  [&](auto &e) {return extension == e.extension;}
              );
    return it == std::cend(extensions) ? INVALID : it->file_type;
}

// A function to determine if a given file is a given type
inline static bool is_type(const std::string &extension, const FileType file_type)
{
    auto it = std::find_if(std::cbegin(extensions), 
                  std::cend(extensions), 
                  [&](auto &e) {return extension == e.extension;}
              );
   return it == std::cend(extensions) || it->file_type != file_type ? false : true;
}

FileType ScanJob::add_directory(std::filesystem::path &path)
{
    std::set<FileType> extensions;
    std::vector<std::string> file_list;
    FileType file_type;
    size_t num_extensions;

    // Determine directory filetype
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(path)) {
        if (!entry.is_regular_file() || !entry.path().has_extension()) {
            continue;
        }
        file_type = determine_filetype(entry.path().extension().string());
        if (file_type != INVALID) {
            extensions.insert(file_type);
        }
    }
    num_extensions = extensions.size();
    if (num_extensions != 1) {
        return INVALID;
    }
    file_type = *extensions.begin();

    // Generate vector of files with directory file type
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(path)) {
        if (!entry.is_regular_file() || !entry.path().has_extension()) {
            continue;
        }
        if (is_type(entry.path().extension().string(), file_type)) {
            tracks.push_back(Track(entry.path().string(), file_type));
        }
    }
    type = file_type;
    nb_files = tracks.size();
    this->path = path.string();
    return nb_files ? file_type : INVALID;
}

int ScanJob::add_files(char **files, int nb_files)
{
    FileType file_type;
    std::filesystem::path path;
    for (int i = 0; i < nb_files; i++) {
        path = files[i];
        file_type = determine_filetype(path.extension().string());
        if (file_type == INVALID) {
            output_error("File '{}' is not of a supported type", files[i]);
        }
        else {
            tracks.push_back(Track(path.string(), file_type));
        }
    }
    this->nb_files = tracks.size();
    return this->nb_files ? 0 : 1;
}

void free_ebur128(ebur128_state *ebur128_state)
{
    if (ebur128_state != NULL)
        ebur128_destroy(&ebur128_state);
}

bool ScanJob::scan(const Config &config, std::mutex *ffmpeg_mutex)
{
    for (Track &track : tracks) {
        error = track.scan(config, ffmpeg_mutex);
        if (error)
            return true;
    }

    if (config.tag_mode != 'd')
        this->calculate_loudness(config);

    this->tag_tracks(config);
    return false;
}

bool Track::scan(const Config &config, std::mutex *m)
{
    ProgressBar progress_bar;
    int rc, stream_id = -1;
    int start = 0, len = 0, pos = 0;
    uint8_t *swr_out_data[1];
    bool error = false;
    int peak_mode;
    bool output_progress = !quiet && !multithread && config.tag_mode != 'd';
    std::unique_lock<std::mutex> *lk = NULL;
    ebur128_state *ebur128 = NULL;

    // FFmpeg 5.0 workaround
#if LIBAVCODEC_VERSION_MAJOR >= 59 
    const AVCodec *codec = NULL;
#else
    AVCodec *codec = NULL;
#endif
    AVPacket *packet = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVFrame *frame = NULL;
    SwrContext *swr = NULL;
    AVFormatContext *format_ctx = NULL;

    // For Opus files, FFmpeg always adjusts the decoded audio samples by the header output
    // gain with no way to disable. To get the actual loudness of the audio signal,
    // we need to set the header output gain to 0 dB before decoding
    if (type == OPUS && config.tag_mode != 's')
        set_opus_header_gain(path.c_str(), 0);
    
    if (m != NULL)
        lk = new std::unique_lock<std::mutex>(*m, std::defer_lock);
    if (output_progress)
        output_ok("Scanning '{}'", path);

    if (lk != NULL)
        lk->lock();
    av_log_set_callback(scan_av_log);
    rc = avformat_open_input(&format_ctx, path.c_str(), NULL, NULL);
    if (rc < 0) {
        char errbuf[256];
        av_strerror(rc, errbuf, sizeof(errbuf));
        output_error("Could not open input: '{}'", errbuf);
        error = true;
        goto end;
    }

    container = format_ctx->iformat->name;
    if (output_progress)
        output_ok("Container: {} [{}]", format_ctx->iformat->long_name, format_ctx->iformat->name);

    rc = avformat_find_stream_info(format_ctx, NULL);
    if (rc < 0) {
        char errbuf[256];
        av_strerror(rc, errbuf, sizeof(errbuf));
        output_error("Could not find stream info: {}", errbuf);
        error = true;
        goto end;
    }

    // Select the best audio stream
    stream_id = av_find_best_stream(format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (stream_id < 0) {
        output_error("Could not find audio stream");
        error = true;
        goto end;
    }
        
    // Create the codec context
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        output_error("Could not allocate audio codec context");
        error = true;
        goto end;
    }
    avcodec_parameters_to_context(codec_ctx, format_ctx->streams[stream_id]->codecpar);

    // Initialize the decoder
    rc = avcodec_open2(codec_ctx, codec, NULL);
    if (rc < 0) {
        char errbuf[256];
        av_strerror(rc, errbuf, sizeof(errbuf));
        output_error("Could not open codec: '{}'", errbuf);
        error = true;
        goto end;
    }
    codec_id = codec->id;

    // For delete tags mode, we don't need to actually scan the file, we only
    // need the codec id to know how to properly handle the file tagging
    if (config.tag_mode == 'd')
        goto end;

    // Try to get default channel layout
    if (!codec_ctx->channel_layout)
        codec_ctx->channel_layout = av_get_default_channel_layout(codec_ctx->channels);

    // Display some information about the file
    if (output_progress)
        output_ok("Stream #{}: {}, {}{:L} Hz, {} ch",
            stream_id, 
            codec->long_name, 
            codec_ctx->bits_per_raw_sample > 0 ? fmt::format("{} bit, ", codec_ctx->bits_per_raw_sample) : "", 
            codec_ctx->sample_rate, 
            codec_ctx->channels
        );

    // Only initialize swresample if we need to convert the format
    if (codec_ctx->sample_fmt != OUTPUT_FORMAT) {
        swr = swr_alloc();
        av_opt_set_channel_layout(swr, "in_channel_layout", codec_ctx->channel_layout, 0);
        av_opt_set_channel_layout(swr, "out_channel_layout", codec_ctx->channel_layout, 0);
        av_opt_set_int(swr, "in_channel_count",  codec_ctx->channels, 0);
        av_opt_set_int(swr, "out_channel_count", codec_ctx->channels, 0);
        av_opt_set_int(swr, "in_sample_rate", codec_ctx->sample_rate, 0);
        av_opt_set_int(swr, "out_sample_rate", codec_ctx->sample_rate, 0);
        av_opt_set_sample_fmt(swr, "in_sample_fmt", codec_ctx->sample_fmt, 0);
        av_opt_set_sample_fmt(swr, "out_sample_fmt", OUTPUT_FORMAT, 0);

        rc = swr_init(swr);
        if (rc < 0) {
            char errbuf[256];
            av_strerror(rc, errbuf, sizeof(errbuf));
            output_error("Could not open libswresample context: {}", errbuf);
            error = true;
            goto end;
        }
    }

    if (lk != NULL)
        lk->unlock();

    // Initialize libebur128
    peak_mode = config.true_peak ? EBUR128_MODE_TRUE_PEAK : EBUR128_MODE_SAMPLE_PEAK;
    ebur128 = ebur128_init(codec_ctx->channels, codec_ctx->sample_rate, EBUR128_MODE_I | peak_mode);
    if (ebur128 == NULL) {
        output_error("Could not initialize libebur128 scanner");
        error = true;
        goto end;
    }

    // Allocate AVPacket structure
    packet = av_packet_alloc();
    if (packet == NULL) {
        output_error("Could not allocate packet");
        error = true;
        goto end;
    }

    // Alocate AVFrame structure
    frame = av_frame_alloc();
    if (frame == NULL) {
        output_error("Could not allocate frame");
        error = true;
        goto end;
    }

    if (output_progress) { 
        if (format_ctx->streams[stream_id]->start_time == AV_NOPTS_VALUE || 
        format_ctx->streams[stream_id]->duration == AV_NOPTS_VALUE) {
            output_progress = false;
        }
        else {
            start = format_ctx->streams[stream_id]->start_time * av_q2d(format_ctx->streams[stream_id]->time_base);
            len  = format_ctx->streams[stream_id]->duration * av_q2d(format_ctx->streams[stream_id]->time_base);
            progress_bar.begin(start, len);
        }
    }
    
    while (av_read_frame(format_ctx, packet) == 0) {
        if (packet->stream_index == stream_id) {
            if (avcodec_send_packet(codec_ctx, packet) == 0) {
                if (avcodec_receive_frame(codec_ctx, frame) == 0) {
                    pos = frame->pkt_dts*av_q2d(format_ctx->streams[stream_id]->time_base);

                    // Convert audio format with libswresample if necessary
                    if (swr != NULL) {
                        size_t out_size = av_samples_get_buffer_size(NULL, 
                                              frame->channels, 
                                              frame->nb_samples, 
                                              OUTPUT_FORMAT, 
                                              0
                                          );
                        swr_out_data[0] = (uint8_t*) av_malloc(out_size);

                        if (swr_convert(swr, swr_out_data, frame->nb_samples, (const uint8_t**) frame->data, frame->nb_samples) < 0) {
                            output_error("Could not convert audio frame");
                            error = true;
                            av_free(swr_out_data[0]);
                            goto end;
                        }
                    
                        rc = ebur128_add_frames_short(ebur128, (short*) swr_out_data[0], frame->nb_samples);         
                        av_free(swr_out_data[0]);                        
                    }

                    // Audio is already in correct format
                    else {
                        ebur128_add_frames_short(ebur128, (short*) frame->data[0], frame->nb_samples);
                    }
                    
                    if (output_progress && pos >= 0)
                        progress_bar.update(pos);
                }
                av_frame_unref(frame);
            }
        }
        av_packet_unref(packet);
    }

    // Make sure the progress bar finishes at 100%
    if (output_progress)
        progress_bar.complete();

end:
    av_packet_free(&packet);
    av_frame_free(&frame);
    if (codec_ctx != NULL)
        avcodec_free_context(&codec_ctx);
    if (format_ctx != NULL)
        avformat_close_input(&format_ctx);
    if (swr != NULL) {
        swr_close(swr);
        swr_free(&swr);
    }

    // Use a smart pointer to manage the remaining lifetime of the ebur128 state
    if (ebur128 != NULL) {
        std::unique_ptr<ebur128_state, void (*)(ebur128_state*)> tmp(ebur128, free_ebur128);
        this->ebur128 = std::move(tmp);
    }
    
    delete lk;
    return error;
}

void ScanJob::calculate_loudness(const Config &config)
{
    // Track loudness calculations
    for (auto track = tracks.begin(); track != tracks.end();) {
        if (track->calculate_loudness(config)) {
            tracks.erase(track);
            nb_files--;
        }
        else {
            ++track;
        }
    }

    // Album loudness calculations
    if (config.do_album)
        this->calculate_album_loudness(config);

    // Check clipping conditions
    if (config.clip_mode != 'n') {
        double t_new_peak; // Track peak after application of gain
        double a_new_peak; // Album peak after application of gain
        double max_peak = pow(10.0, config.max_peak_level / 20.f);

        // Track clipping
        for (Track &track : tracks) {
            if (config.clip_mode == 'a' || (config.clip_mode == 'p' && (track.result.track_gain > 0.f))) {
                t_new_peak = pow(10.0, track.result.track_gain / 20.f) * track.result.track_peak;
                if (t_new_peak > max_peak) {
                    double adjustment = 20.f * log10(t_new_peak / max_peak);
                    if (config.clip_mode == 'p' && adjustment > track.result.track_gain)
                        adjustment = track.result.track_gain;
                    track.result.track_gain -= adjustment;
                    track.tclip = true;
                }
            }
        }

        // Album clipping
        double album_gain = tracks[0].result.album_gain;
        double album_peak = tracks[0].result.album_peak;
        if (config.do_album && (config.clip_mode == 'a' || (config.clip_mode == 'p' && album_gain > 0.f))) {
            a_new_peak = pow(10.0, album_gain / 20.f) * album_peak;
            if (a_new_peak > max_peak) {
                double adjustment = 20.f * log10(a_new_peak / max_peak);
                if (config.clip_mode == 'p' && adjustment > album_gain)
                    adjustment = album_gain;
                for (Track &track : tracks) {
                    track.result.album_gain -= adjustment;
                    track.aclip = true;
                }
            }
        }
    }
}

void ScanJob::tag_tracks(const Config &config)
{
    std::FILE *stream = NULL;
    if (config.tab_output != TYPE_NONE) {
        if (config.tab_output == TYPE_FILE) {
            std::filesystem::path output_file = std::filesystem::path(path) / "replaygain.csv";
            stream = fopen(output_file.string().c_str(), "wb");
        }
        else {
            stream = stdout;
        }
        if (stream != NULL)
            fputs("Filename\tLoudness (LUFS)\tGain (dB)\tPeak\t Peak (dB)\tPeak Type\tClipping Adjustment?\n", stream);
    }

    // Tag the files
    bool tab_output = config.tab_output != TYPE_NONE && stream != NULL;
    bool human_output = !multithread && !quiet && config.tag_mode != 'd';
    for (Track &track : tracks) {
        if (config.tag_mode != 's')
            tag_track(track, config);

        if (tab_output) {
            // Filename;Loudness;Gain (dB);Peak;Peak (dB);Peak Type;Clipping Adjustment;
            fmt::print(stream, "{}\t", std::filesystem::path(track.path).filename().string());
            fmt::print(stream, "{:.2f}\t", track.result.track_loudness);
            fmt::print(stream, "{:.2f}\t", track.result.track_gain);
            fmt::print(stream, "{:.6f}\t", track.result.track_peak);
            fmt::print(stream, "{:.2f}\t", 20.0 * log10(track.result.track_peak));
            fmt::print(stream, "{}\t", config.true_peak ? "True" : "Sample");
            fmt::print(stream, "{}\t", track.tclip ? "Y" : "N");
            fmt::print(stream, "\n");
            if (config.do_album && ((&track - &tracks[0]) == (nb_files - 1))) {
                fmt::print(stream, "{}\t", "Album");
                fmt::print(stream, "{:.2f}\t", track.result.album_loudness);
                fmt::print(stream, "{:.2f}\t", track.result.album_gain);
                fmt::print(stream, "{:.6f}\t", track.result.album_peak);
                fmt::print(stream, "{:.2f}\t", 20.0 * log10(track.result.album_peak));
                fmt::print(stream, "{}\t", config.true_peak ? "True" : "Sample");
                fmt::print(stream, "{}\n", track.aclip ? "Y" : "N");
                fmt::print(stream, "\n");
            }
        } 
        
        // Human-readable output
        if (human_output) {
            fmt::print("\nTrack: {}\n", track.path);
            fmt::print("  Loudness: {:8.2f} LUFS\n", track.result.track_loudness);
            fmt::print("  Peak:     {:8.6f} ({:.2f} dB)\n", track.result.track_peak, 20.0 * log10(track.result.track_peak));
            fmt::print("  Gain:     {:8.2f} dB {}{}\n", 
                track.result.track_gain,
                track.type == OPUS && config.opus_mode != 'd' ? fmt::format("({})", GAIN_TO_Q78(track.result.track_gain)) : "",
                track.tclip ? " (adjusted to prevent clipping)" : ""
            );

            if (config.do_album && ((&track - &tracks[0]) == (nb_files - 1))) {
                fmt::print("\nAlbum:\n");
                fmt::print("  Loudness: {:8.2f} LUFS\n", track.result.album_loudness);
                fmt::print("  Peak:     {:8.6f} ({:.2f} dB)\n", track.result.album_peak, 20.0 * log10(track.result.album_peak));
                fmt::print("  Gain:     {:8.2f} dB {}{}\n", 
                    track.result.album_gain,
                    track.type == OPUS && config.opus_mode != 'd' ? fmt::format("({})", GAIN_TO_Q78(track.result.album_gain)) : "",
                    track.tclip ? " (adjusted to prevent clipping)" : ""
                );
            }
            fmt::print("\n");
        }
    }
    if (config.tab_output == TYPE_FILE && stream != NULL)
        fclose(stream);
}

void ScanJob::update_data(ScanData &data)
{
    if (error) {
        data.error_directories.push_back(path);
        return;
    }
    data.files += nb_files;

    // Collect clipping stats
    for (const Track &track : tracks) {
        if (track.aclip || track.tclip)
            data.clipping_adjustments++;
    }

    for (const Track &track : tracks) {
        data.total_gain += track.result.track_gain;
        data.total_peak += track.result.track_peak;
        track.result.track_gain < 0.f ? data.total_negative++ : data.total_positive++;
    }
}

int Track::calculate_loudness(const Config &config) {
    unsigned channel = 0;
    double track_loudness, track_peak;
    ebur128_state *ebur128 = this->ebur128.get();

    if (ebur128_loudness_global(ebur128, &track_loudness) != EBUR128_SUCCESS)
        track_loudness = config.target_loudness;

    if (track_loudness == -HUGE_VAL) // Don't bother tagging silent tracks
        return 1;

    std::vector<double> peaks(ebur128->channels);
    int (*get_peak)(ebur128_state*, unsigned int, double*) = config.true_peak ? ebur128_true_peak : ebur128_sample_peak;
    for (double &pk : peaks)
        get_peak(ebur128, channel++, &pk);
    track_peak = *std::max_element(peaks.begin(), peaks.end());

    result.track_gain           = config.target_loudness - track_loudness;
    result.track_peak           = track_peak;
    result.track_loudness       = track_loudness;
    return 0;
}


void ScanJob::calculate_album_loudness(const Config &config) {
    double album_loudness, album_peak;
    int nb_states = tracks.size();
    std::vector<ebur128_state*> states(nb_states);
    for (const Track &track : tracks)
        states[&track - &tracks[0]] = track.ebur128.get();

    if (ebur128_loudness_global_multiple(states.data(), nb_states, &album_loudness) != EBUR128_SUCCESS)
        album_loudness = config.target_loudness;

    album_peak = std::max_element(tracks.begin(),
                     tracks.end(),
                     [](auto &a, auto &b) {return a.result.track_peak < b.result.track_peak;}
                 )->result.track_peak;
    
    double album_gain = config.target_loudness - album_loudness;
    for (Track &track : tracks) {
        track.result.album_gain = album_gain;
        track.result.album_peak = album_peak;
        track.result.album_loudness = album_loudness;
    }
}

static void scan_av_log(void *avcl, int level, const char *fmt, va_list args) {

}