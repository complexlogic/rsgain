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
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <filesystem>
#include <unordered_map>
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
#include "easymode.hpp"
#include "scan.hpp"
#include "output.hpp"
#include "tag.hpp"

#define output_fferror(e, msg) char errbuf[256]; av_strerror(e, errbuf, sizeof(errbuf)); output_error(msg ": {}", errbuf)
#define OLD_CHANNEL_LAYOUT LIBAVUTIL_VERSION_MAJOR < 57 || (LIBAVUTIL_VERSION_MAJOR == 57 && LIBAVUTIL_VERSION_MINOR < 18)
#define OUTPUT_FORMAT AV_SAMPLE_FMT_S16

extern bool multithread;

// A function to determine a file type
static FileType determine_filetype(const std::string &extension)
{
    static const std::unordered_map<std::string, FileType> map =  {
        {".mp2",  FileType::MP2},
        {".mp3",  FileType::MP3},
        {".flac", FileType::FLAC},
        {".ogg",  FileType::OGG},
        {".oga",  FileType::OGG},
        {".spx",  FileType::OGG},
        {".opus", FileType::OPUS},
        {".m4a",  FileType::M4A},
	    {".mp4",  FileType::M4A},
        {".wma",  FileType::WMA},
        {".wav",  FileType::WAV},
        {".aiff", FileType::AIFF},
        {".aif",  FileType::AIFF},
        {".snd",  FileType::AIFF},
        {".wv",   FileType::WAVPACK},
        {".ape",  FileType::APE},
        {".tak",  FileType::TAK},
        {".mpc",  FileType::MPC}
    };
	std::string extensionlower = extension;
	std::transform(extensionlower.begin(), extensionlower.end(), extensionlower.begin(), ::tolower);
    auto it = map.find(extensionlower);
    return it == map.end() ? FileType::INVALID : it->second;
}

ScanJob* ScanJob::factory(const std::filesystem::path &path)
{
    std::unordered_set<FileType> extensions;
    FileType file_type;
    std::vector<Track> tracks;

    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(path)) {
        if (entry.is_regular_file() && entry.path().has_extension()
        && ((file_type = determine_filetype(entry.path().extension().string())) != FileType::INVALID)
        && !(file_type == FileType::M4A && get_config(file_type).skip_mp4 && entry.path().extension().string() == ".mp4")) {
            tracks.emplace_back(entry.path(), file_type);
            extensions.insert(file_type);
        }
    }
    if (tracks.empty())
        return nullptr;
    file_type = extensions.size() > 1 ? FileType::DEFAULT : *extensions.begin();
    const Config &config = get_config(file_type);
    if (config.tag_mode == 'n')
        return nullptr;
    return new ScanJob(path, tracks, config, file_type);
}

ScanJob* ScanJob::factory(char **files, size_t nb_files, const Config &config)
{
    FileType file_type;
    std::filesystem::path path;
    std::vector<Track> tracks;
    std::unordered_set<FileType> types;
    for (size_t i = 0; i < nb_files; i++) {
        path = files[i];
        if (!std::filesystem::exists(path)) {
            output_error("File '{}' does not exist", path.string());
            return nullptr;
        }
        else if ((file_type = determine_filetype(path.extension().string())) == FileType::INVALID) {
            output_error("File '{}' is not of a supported type", files[i]);
            return nullptr;
        }
        else {
            tracks.emplace_back(path, file_type);
            types.insert(file_type);
        }
    }
    if (tracks.empty())
        return nullptr;
    return new ScanJob(tracks, config, types.size() > 1 ? FileType::DEFAULT : *types.begin());
}

void free_ebur128(ebur128_state *ebur128_state)
{
    if (ebur128_state)
        ebur128_destroy(&ebur128_state);
}

bool ScanJob::scan(std::mutex *ffmpeg_mutex)
{
    if (config.tag_mode != 'd') {
        if (config.skip_existing) {
            std::vector<int> existing;
            for (auto track = tracks.rbegin(); track != tracks.rend(); ++track) {
                if (tag_exists(*track))
                    existing.push_back((int) (tracks.rend() - track - 1));
            }
            size_t nb_exists = existing.size();
            if (nb_exists) {
                if (nb_exists == tracks.size()) {
                    nb_files = 0;
                    skipped = nb_exists;
                    return true;
                }
                else if (!config.do_album) {
                    for (int i : existing) {
                        tracks.erase(tracks.begin() + i);
                        skipped++;
                        nb_files--;
                    }
                }
            }
        }
        for (Track &track : tracks) {
            error = !track.scan(config, ffmpeg_mutex);
            if (error)
                return false;
        }
        calculate_loudness();
    }

    tag_tracks();
    return true;
}

bool ScanJob::Track::scan(const Config &config, std::mutex *m)
{
    ProgressBar progress_bar;
    int rc, stream_id = -1;
    uint8_t *swr_out_data[1];
    bool ret = false;
    bool repeat = false;
    int peak_mode;
    double time_base;
    bool output_progress = !quiet && !multithread && config.tag_mode != 'd';
    std::unique_lock<std::mutex> *lk = nullptr;
    ebur128_state *ebur128 = nullptr;
    int nb_channels;

#if LIBAVCODEC_VERSION_MAJOR >= 59 
    const 
#endif
    AVCodec *codec = nullptr;
    AVPacket *packet = nullptr;
    AVCodecContext *codec_ctx = nullptr;
    AVFrame *frame = nullptr;
    SwrContext *swr = nullptr;
    AVFormatContext *format_ctx = nullptr;
    const AVStream *stream = nullptr;

    // For Opus files, FFmpeg always adjusts the decoded audio samples by the header output
    // gain with no way to disable. To get the actual loudness of the audio signal,
    // we need to set the header output gain to 0 dB before decoding
    if (type == FileType::OPUS && config.tag_mode != 's')
        set_opus_header_gain(path.string().c_str(), 0);
    
    if (m)
        lk = new std::unique_lock<std::mutex>(*m, std::defer_lock);
    if (output_progress)
        output_ok("Scanning '{}'", path.string());

    if (lk)
        lk->lock();
    rc = avformat_open_input(&format_ctx, format("file:{}", path.string()).c_str(), nullptr, nullptr);
    if (rc < 0) {
        output_fferror(rc, "Could not open input");
        goto end;
    }

    container = format_ctx->iformat->name;
    if (output_progress)
        output_ok("Container: {} [{}]", format_ctx->iformat->long_name, format_ctx->iformat->name);

    rc = avformat_find_stream_info(format_ctx, nullptr);
    if (rc < 0) {
        output_fferror(rc, "Could not find stream info");
        goto end;
    }

    // Select the best audio stream
    stream_id = av_find_best_stream(format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (stream_id < 0) {
        output_error("Could not find audio stream");
        goto end;
    }
    stream = format_ctx->streams[stream_id];
    time_base = av_q2d(stream->time_base);
        
    // Initialize the decoder
    do {
        codec_ctx = avcodec_alloc_context3(codec);
        if (!codec_ctx) {
            output_error("Could not allocate audio codec context");
            goto end;
        }
        avcodec_parameters_to_context(codec_ctx, stream->codecpar);
        rc = avcodec_open2(codec_ctx, codec, nullptr);
        if (rc < 0) {
            if (!repeat) {
#if LIBAVCODEC_VERSION_MAJOR >= 59 
                const
#endif
                AVCodec *try_codec;
                avcodec_free_context(&codec_ctx);
                codec_ctx = nullptr;

                // For AAC files, try the Fraunhofer decoder if the native FFmpeg decoder failed
                if (codec->id == AV_CODEC_ID_AAC) {
                    try_codec = avcodec_find_decoder_by_name("libfdk_aac");
                    if (try_codec) {
                        codec = try_codec;
                        repeat = true;
                        continue;
                    }
                }
            }
            output_fferror(rc, "Could not open codec");
            goto end;
        }
        repeat = false;
    } while (repeat);
    codec_id = codec->id;
#if OLD_CHANNEL_LAYOUT
    nb_channels = codec_ctx->channels;
#else
    nb_channels = codec_ctx->ch_layout.nb_channels;
#endif

    // Display some information about the file
    if (output_progress)
        output_ok("Stream #{}: {}, {}{:L} Hz, {} ch",
            stream_id, 
            codec->long_name, 
            codec_ctx->bits_per_raw_sample > 0 ? format("{} bit, ", codec_ctx->bits_per_raw_sample) : "", 
            codec_ctx->sample_rate, 
            nb_channels
        );

    // Only initialize swresample if we need to convert the format
    if (codec_ctx->sample_fmt != OUTPUT_FORMAT) {
#if OLD_CHANNEL_LAYOUT
        if (!codec_ctx->channel_layout)
            codec_ctx->channel_layout = av_get_default_channel_layout(codec_ctx->channels);
        swr = swr_alloc_set_opts(nullptr,
                 codec_ctx->channel_layout,
                 OUTPUT_FORMAT,
                 codec_ctx->sample_rate,
                 codec_ctx->channel_layout,
                 codec_ctx->sample_fmt,
                 codec_ctx->sample_rate,
                 0,
                 nullptr
             );
#else
        swr_alloc_set_opts2(&swr,
            &codec_ctx->ch_layout,
            OUTPUT_FORMAT,
            codec_ctx->sample_rate,
            &codec_ctx->ch_layout,
            codec_ctx->sample_fmt,
            codec_ctx->sample_rate,
            0,
            nullptr
        );
#endif
        if (!swr) {
            output_error("Could not allocate libswresample context");
            goto end;
        }

        rc = swr_init(swr);
        if (rc < 0) {
            output_fferror(rc, "Could not open libswresample context");
            goto end;
        }
    }

    if (lk)
        lk->unlock();

    // Initialize libebur128
    peak_mode = config.true_peak ? EBUR128_MODE_TRUE_PEAK : EBUR128_MODE_SAMPLE_PEAK;
    ebur128 = ebur128_init((unsigned int) nb_channels,
        (size_t) codec_ctx->sample_rate,
        EBUR128_MODE_I | peak_mode
    );
    if (!ebur128) {
        output_error("Could not initialize libebur128 scanner");
        goto end;
    }

    // Allocate AVPacket structure
    packet = av_packet_alloc();
    if (!packet) {
        output_error("Could not allocate packet");
        goto end;
    }

    // Alocate AVFrame structure
    frame = av_frame_alloc();
    if (!frame) {
        output_error("Could not allocate frame");
        goto end;
    }

    if (output_progress) { 
        if (stream->duration == AV_NOPTS_VALUE)
            output_progress = false;
        else {
            int start = 0;
            if (stream->start_time != AV_NOPTS_VALUE)
                start = (int) std::round((double) stream->start_time * time_base);
            progress_bar.begin(start, (int) std::round((double) stream->duration * time_base));
        }
    }
    
    while (av_read_frame(format_ctx, packet) == 0) {
        if (packet->stream_index == stream_id) {
            if ((rc = avcodec_send_packet(codec_ctx, packet)) == 0) {
                while ((rc = avcodec_receive_frame(codec_ctx, frame)) >= 0) {
#if OLD_CHANNEL_LAYOUT
                    if (frame->channels == nb_channels) {
#else
                    if (frame->ch_layout.nb_channels == nb_channels) {
#endif
                        // Convert audio format with libswresample if necessary
                        if (swr) {
                            size_t out_size = static_cast<size_t>(
                                av_samples_get_buffer_size(nullptr,
                                    nb_channels,
                                    frame->nb_samples,
                                    OUTPUT_FORMAT,
                                    0
                                )
                            );
                            swr_out_data[0] = (uint8_t*) av_malloc(out_size);
                            if (swr_convert(swr, swr_out_data, frame->nb_samples, (const uint8_t**) frame->data, frame->nb_samples) < 0) {
                                output_error("Could not convert audio frame");
                                av_free(swr_out_data[0]);
                                goto end;
                            }

                            ebur128_add_frames_short(ebur128, (short*) swr_out_data[0], static_cast<size_t>(frame->nb_samples));
                            av_free(swr_out_data[0]);
                        }

                        // Audio is already in correct format
                        else
                            ebur128_add_frames_short(ebur128, (short*) frame->data[0], static_cast<size_t>(frame->nb_samples));

                        if (output_progress) {
                            int pos = (int) std::round((double) frame->pts * time_base);
                            if (pos >= 0)
                                progress_bar.update(pos);
                        }
                    }
                    av_frame_unref(frame);
                }
            }
        }
        av_packet_unref(packet);
    }

    // Make sure the progress bar finishes at 100%
    if (output_progress)
        progress_bar.complete();

    ret = true;
end:
    av_packet_free(&packet);
    av_frame_free(&frame);
    if (codec_ctx)
        avcodec_free_context(&codec_ctx);
    if (format_ctx)
        avformat_close_input(&format_ctx);
    if (swr)
        swr_free(&swr);

    // Use a smart pointer to manage the remaining lifetime of the ebur128 state
    if (ebur128) 
        this->ebur128 = std::unique_ptr<ebur128_state, decltype(&free_ebur128)>(ebur128, free_ebur128);
    
    delete lk;
    return ret;
}

void ScanJob::calculate_loudness()
{
    if (tracks.empty())
        return;

    // Track loudness calculations
    for (Track &track : tracks)
        track.calculate_loudness(config);

    // Album loudness calculations
    if (config.do_album)
        calculate_album_loudness();

    // Check clipping conditions
    if (config.clip_mode != 'n') {
        double t_new_peak; // Track peak after application of gain
        double a_new_peak; // Album peak after application of gain
        double max_peak = pow(10.0, config.max_peak_level / 20.0);

        // Track clipping
        for (Track &track : tracks) {
            if (config.clip_mode == 'a' || (config.clip_mode == 'p' && (track.result.track_gain > 0.0))) {
                t_new_peak = pow(10.0, track.result.track_gain / 20.0) * track.result.track_peak;
                if (t_new_peak > max_peak) {
                    double adjustment = 20.0 * log10(t_new_peak / max_peak);
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
        if (config.do_album && (config.clip_mode == 'a' || (config.clip_mode == 'p' && album_gain > 0.0))) {
            a_new_peak = pow(10.0, album_gain / 20.0) * album_peak;
            if (a_new_peak > max_peak) {
                double adjustment = 20.0 * log10(a_new_peak / max_peak);
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

void ScanJob::tag_tracks()
{
    if (tracks.empty())
        return;
    std::FILE *stream = nullptr;
    if (config.tab_output != OutputType::NONE) {
        if (config.tab_output == OutputType::FILE) {
            std::filesystem::path output_file = path / "replaygain.csv";
            stream = fopen(output_file.string().c_str(), "wb");
        }
        else
            stream = stdout;

        if (stream) {
            if (config.sep_header)
                fputs("sep=\t\n", stream);
            fputs("Filename\tLoudness (LUFS)\tGain (dB)\tPeak\t Peak (dB)\tPeak Type\tClipping Adjustment?\n", stream);
        }
    }

    // Tag the files
    bool tab_output = config.tab_output != OutputType::NONE && stream != nullptr;
    bool human_output = !multithread && !quiet && config.tag_mode != 'd';
    if (config.sort_alphanum)
        std::sort(tracks.begin(), tracks.end(), [](const auto &a, const auto &b){ return a.path.string() < b.path.string(); });
    for (Track &track : tracks) {
        if (config.tag_mode != 's')
            tag_track(track, config);

        if (tab_output) {
            // Filename;Loudness;Gain (dB);Peak;Peak (dB);Peak Type;Clipping Adjustment;
            print(stream, "{}\t", track.path.filename().string());
            track.result.track_loudness == -HUGE_VAL ? print(stream, "-∞\t") : print(stream, "{:.2f}\t", track.result.track_loudness);
            print(stream, "{:.2f}\t", track.result.track_gain);
            print(stream, "{:.6f}\t", track.result.track_peak);
            track.result.track_peak == 0.0 ? print(stream, "-∞\t") : print(stream, "{:.2f}\t", 20.0 * log10(track.result.track_peak));
            print(stream, "{}\t", config.true_peak ? "True" : "Sample");
            print(stream, "{}\n", track.tclip ? "Y" : "N");
            if (config.do_album && ((size_t) (&track - &tracks[0]) == (nb_files - 1))) {
                print(stream, "{}\t", "Album");
                track.result.album_loudness == -HUGE_VAL ? print(stream, "-∞\t") : print(stream, "{:.2f}\t", track.result.album_loudness);
                print(stream, "{:.2f}\t", track.result.album_gain);
                print(stream, "{:.6f}\t", track.result.album_peak);
                track.result.album_peak == 0.0 ? print(stream, "-∞\t") : print(stream, "{:.2f}\t", 20.0 * log10(track.result.album_peak));
                print(stream, "{}\t", config.true_peak ? "True" : "Sample");
                print(stream, "{}\n", track.aclip ? "Y" : "N");
            }
        } 
        
        // Human-readable output
        if (human_output) {
            print("\nTrack: {}\n", track.path.string());
            print("  Loudness: {} LUFS\n", track.result.track_loudness == -HUGE_VAL ? "   -∞" : format("{:8.2f}", track.result.track_loudness));
            print("  Peak:     {:8.6f} ({} dB)\n",
                track.result.track_peak,
                track.result.track_peak == 0.0 ? "-∞" : format("{:.2f}", 20.0 * log10(track.result.track_peak))
            );
            print("  Gain:     {:8.2f} dB {}{}\n", 
                track.result.track_gain,
                track.type == FileType::OPUS && (config.opus_mode == 'r' || config.opus_mode == 's') ? format("({})", GAIN_TO_Q78(track.result.track_gain)) : "",
                track.tclip ? " (adjusted to prevent clipping)" : ""
            );

            if (config.do_album && ((size_t) (&track - &tracks[0]) == (nb_files - 1))) {
                print("\nAlbum:\n");
                print("  Loudness: {} LUFS\n", track.result.album_loudness == -HUGE_VAL ? "   -∞" : format("{:8.2f}", track.result.album_loudness));
                print("  Peak:     {:8.6f} ({} dB)\n",
                    track.result.album_peak,
                    track.result.album_peak == 0.0 ? "-∞" : format("{:.2f}", 20.0 * log10(track.result.album_peak))
                );
                print("  Gain:     {:8.2f} dB {}{}\n", 
                    track.result.album_gain,
                    type == FileType::OPUS && (config.opus_mode == 'r' || config.opus_mode == 's') ? format("({})", GAIN_TO_Q78(track.result.album_gain)) : "",
                    track.aclip ? " (adjusted to prevent clipping)" : ""
                );
            }
            print("\n");
        }
    }
    if (config.tab_output == OutputType::FILE && stream != nullptr)
        fclose(stream);
}

void ScanJob::update_data(ScanData &data)
{
    if (error) {
        data.error_directories.push_back(path.string());
        return;
    }
    data.files += nb_files;
    data.skipped += skipped;
    if (!nb_files)
        return;

    // Collect clipping stats
    for (const Track &track : tracks) {
        if (track.aclip || track.tclip)
            data.clipping_adjustments++;
    }

    if (config.tag_mode != 'd') {
        for (const Track &track : tracks) {
            data.total_gain += track.result.track_gain;
            data.total_peak += track.result.track_peak;
            track.result.track_gain > 0.0 ? data.total_positive++ : data.total_negative++;
        }
    }
}

void ScanJob::Track::calculate_loudness(const Config &config)
{
    unsigned int channel = 0;
    double track_loudness, track_peak;

    if (ebur128_loudness_global(ebur128.get(), &track_loudness) != EBUR128_SUCCESS)
        track_loudness = config.target_loudness;

    // Edge case for completely silent tracks
    if (track_loudness == -HUGE_VAL) {
        result.track_gain = 0.0;
        result.track_peak = 0.0;
        result.track_loudness = -HUGE_VAL;
    }

    else {
        std::vector<double> peaks(ebur128->channels);
        int (*get_peak)(ebur128_state*, unsigned int, double*) = config.true_peak ? ebur128_true_peak : ebur128_sample_peak;
        for (double &pk : peaks)
            get_peak(ebur128.get(), channel++, &pk);
        track_peak = *std::max_element(peaks.begin(), peaks.end());

        result.track_gain = (type == FileType::OPUS && config.opus_mode == 's' ? -23.0 : config.target_loudness)
                             - track_loudness;
        result.track_peak = track_peak;
        result.track_loudness = track_loudness;
    }
}

void ScanJob::calculate_album_loudness() 
{
    double album_loudness, album_peak;
    size_t nb_states = tracks.size();
    std::vector<ebur128_state*> states(nb_states);
    for (const Track &track : tracks)
        if (track.result.track_loudness != -HUGE_VAL)
            states.emplace_back(track.ebur128.get());

    if (ebur128_loudness_global_multiple(states.data(), states.size(), &album_loudness) != EBUR128_SUCCESS)
        album_loudness = config.target_loudness;

    album_peak = std::max_element(tracks.begin(),
                     tracks.end(),
                     [](const auto &a, const auto &b) { return a.result.track_peak < b.result.track_peak; }
                 )->result.track_peak;
    
    double album_gain = (type == FileType::OPUS && config.opus_mode == 's' ? -23.0 : config.target_loudness)
                         - album_loudness;
    for (Track &track : tracks) {
        track.result.album_gain = album_gain;
        track.result.album_peak = album_peak;
        track.result.album_loudness = album_loudness;
    }
}
