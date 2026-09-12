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

struct AVPacketDeleter {
    void operator()(AVPacket *packet) const noexcept {
        if (packet)
			av_packet_free(&packet);
    }
};
struct AVFrameDeleter {
    void operator()(AVFrame *frame) const noexcept {
        if (frame)
			av_frame_free(&frame);
    }
};

struct DecoderParams {
#if LIBAVCODEC_VERSION_MAJOR >= 59
    const
#endif
    AVCodec *codec{nullptr};
    AVCodecContext *codec_ctx{nullptr};
    AVFormatContext *format_ctx{nullptr};
    SwrContext *swr{nullptr};
    AVStream *stream{nullptr};
    AVSampleFormat output_format;
    int stream_id{-1};
    int nb_channels{0};
    double time_base{0.0};
    ~DecoderParams()
    {
        if (codec_ctx)
            avcodec_free_context(&codec_ctx);
        if (format_ctx)
            avformat_close_input(&format_ctx);
        if (swr)
            swr_free(&swr);
    }
};

template <typename T>
constexpr void output_fferror(int error, T&& msg)
{
    char errbuf[512];
    av_strerror(error, errbuf, sizeof(errbuf));
    output_error("{}: {}", msg, errbuf);
}
#define OLD_CHANNEL_LAYOUT LIBAVUTIL_VERSION_MAJOR < 57 || (LIBAVUTIL_VERSION_MAJOR == 57 && LIBAVUTIL_VERSION_MINOR < 18)

extern bool multithread;

static AVSampleFormat determine_output_format(AVSampleFormat input)
{
    static const std::unordered_map<AVSampleFormat, AVSampleFormat> map {
        {AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_S16},
        {AV_SAMPLE_FMT_S16P, AV_SAMPLE_FMT_S16},
        {AV_SAMPLE_FMT_S32, AV_SAMPLE_FMT_S32},
        {AV_SAMPLE_FMT_S32P, AV_SAMPLE_FMT_S32},
        {AV_SAMPLE_FMT_FLT, AV_SAMPLE_FMT_FLT},
        {AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_FLT},
        {AV_SAMPLE_FMT_DBL, AV_SAMPLE_FMT_DBL},
        {AV_SAMPLE_FMT_DBLP, AV_SAMPLE_FMT_DBL}
    };
    auto it = map.find(input);
    return it == map.end() ? AV_SAMPLE_FMT_FLT : it->second;
}

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
        {".mpc",  FileType::MPC},
        {".dsf",  FileType::DSF},
#ifdef HAS_MATROSKA
        {".mka",  FileType::MATROSKA},
        {".mkv",  FileType::MATROSKA},
        {".webm", FileType::WEBM}
#endif
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
        && !(file_type == FileType::M4A && get_config(file_type).skip_mp4 && entry.path().extension().string() == ".mp4")
        && !(entry.path().filename().string().starts_with("._"))) {
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
#ifdef _WIN32
        // Windows CommandLineToArgvW treats \" as a literal " rather than closing
        // the argument, so a path typed as "C:\foo\bar\" arrives with a trailing "
        // instead of \. Strip it so filesystem operations work correctly.
        std::string file_str = files[i];
        if (!file_str.empty() && file_str.back() == '"')
            file_str.pop_back();
        path = file_str;
#else
        path = files[i];
#endif
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
        ScanReturn ret;
        std::vector<size_t> remove;
        for (Track &track : tracks) {
            ret = track.scan(config, ffmpeg_mutex);
            if (ret == ScanReturn::ERR) {
                error = true;
                return false;
            }
            else if (ret == ScanReturn::NO_STREAM)
                remove.push_back(&track - &tracks[0]);
        }
        for (auto it = remove.rbegin(); it != remove.rend(); ++it) {
            tracks.erase(tracks.begin() + *it);
            nb_files--;
        }
        calculate_loudness();
    }

    tag_tracks();
    return true;
}

template <typename T, int (*ebur128_add_frames)(ebur128_state* st, const T* src, size_t frames)>
ScanReturn ScanJob::Track::scan_loop(DecoderParams &dp,  ProgressBar *progress_bar)
{
    int rc;
    uint8_t *swr_out_data[1];
    std::unique_ptr<uint8_t[]> buffer;
    size_t buffer_size{};

    // Allocate AVPacket structure
    std::unique_ptr<AVPacket, AVPacketDeleter> packet(av_packet_alloc());
    if (!packet) {
        if (!multithread)
            output_error("Could not allocate packet");
        return ScanReturn::ERR;
    }

    // Alocate AVFrame structure
    std::unique_ptr<AVFrame, AVFrameDeleter> frame(av_frame_alloc());
    if (!frame) {
        if (!multithread)
            output_error("Could not allocate frame");
        return ScanReturn::ERR;
    }

    while (av_read_frame(dp.format_ctx, packet.get()) == 0) {
        if (packet->stream_index == dp.stream_id) {
            if ((rc = avcodec_send_packet(dp.codec_ctx, packet.get())) == 0) {
                while ((rc = avcodec_receive_frame(dp.codec_ctx, frame.get())) >= 0) {
#if OLD_CHANNEL_LAYOUT
                    if (frame->channels == dp.nb_channels) {
#else
                    if (frame->ch_layout.nb_channels == dp.nb_channels) {
#endif
                        // Convert audio format with libswresample if necessary
                        if (dp.swr) {
                            size_t out_size = static_cast<size_t>(
                                av_samples_get_buffer_size(nullptr,
                                    dp.nb_channels,
                                    frame->nb_samples,
                                    dp.output_format,
                                    0
                                )
                            );
                            if (out_size < 0) {
                                output_error("Could not calculate output buffer size");
                                return ScanReturn::ERR;
                            }
                            if (out_size > buffer_size) {
                                buffer = std::make_unique_for_overwrite<uint8_t[]>(out_size);
                                buffer_size = out_size;
                                swr_out_data[0] = buffer.get();
                            }
                            if (swr_convert(dp.swr, swr_out_data, frame->nb_samples, (const uint8_t**) frame->data, frame->nb_samples) < 0) {
                                if (!multithread)
                                    output_error("Could not convert audio frame");
                                return ScanReturn::ERR;
                            }
                            ebur128_add_frames(ebur128.get(), reinterpret_cast<T*>(swr_out_data[0]), static_cast<size_t>(frame->nb_samples));
                        }

                        // Audio is already in correct format
                        else
                            ebur128_add_frames(ebur128.get(), reinterpret_cast<T*>(frame->data[0]), static_cast<size_t>(frame->nb_samples));

                        if (progress_bar) {
                            int pos = (int) std::round((double) frame->pts * dp.time_base);
                            if (pos >= 0)
                                progress_bar->update(pos);
                        }
                    }
                    av_frame_unref(frame.get());
                }
            }
        }
        av_packet_unref(packet.get());
    }

    // Make sure the progress bar finishes at 100%
    if (progress_bar)
        progress_bar->complete();

    return ScanReturn::SUCCESS;
}

ScanReturn ScanJob::Track::scan(const Config &config, std::mutex *m)
{
    std::unique_ptr<ProgressBar> progress_bar;
    int rc;
    bool repeat = false;
    int peak_mode;
    bool output_progress = !quiet && !multithread && config.tag_mode != 'd';
    DecoderParams dp;

    if (config.preserve_mtimes) {
        mtime = std::make_unique<std::filesystem::file_time_type>();
        *mtime = std::filesystem::last_write_time(path);
    }

    // For Opus files, FFmpeg always adjusts the decoded audio samples by the header output
    // gain with no way to disable. To get the actual loudness of the audio signal,
    // we need to set the header output gain to 0 dB before decoding
    if (type == FileType::OPUS && config.tag_mode != 's')
        set_opus_header_gain(path.string().c_str(), 0);

    if (output_progress)
        output_ok("Scanning '{}'", path.string());
    {
        std::unique_ptr<std::scoped_lock<std::mutex>> lk;
        if (m)
            lk = std::make_unique<std::scoped_lock<std::mutex>>(*m);
        rc = avformat_open_input(&dp.format_ctx, rsgain::format("file:{}", path.string()).c_str(), nullptr, nullptr);
        if (rc < 0) {
            if (!multithread)
                output_fferror(rc, "Could not open input");
            return ScanReturn::ERR;
        }

        container = dp.format_ctx->iformat->name;
        if (output_progress)
            output_ok("Container: {} [{}]", dp.format_ctx->iformat->long_name, dp.format_ctx->iformat->name);

        rc = avformat_find_stream_info(dp.format_ctx, nullptr);
        if (rc < 0) {
            if (!multithread)
                output_fferror(rc, "Could not find stream info");
            return ScanReturn::ERR;
        }

        // Select the best audio stream
        dp.stream_id = av_find_best_stream(dp.format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &dp.codec, 0);
        if (dp.stream_id < 0) {
            if (!multithread)
                output_warn("Could not find audio stream\n");
            return ScanReturn::NO_STREAM;
        }
        dp.stream = dp.format_ctx->streams[dp.stream_id];
        dp.time_base = av_q2d(dp.stream->time_base);

        // Initialize the decoder
        do {
            dp.codec_ctx = avcodec_alloc_context3(dp.codec);
            if (!dp.codec_ctx) {
                if (!multithread)
                    output_error("Could not allocate audio codec context");
                return ScanReturn::ERR;
            }
            avcodec_parameters_to_context(dp.codec_ctx, dp.stream->codecpar);
            rc = avcodec_open2(dp.codec_ctx, dp.codec, nullptr);
            if (rc < 0) {
                if (!repeat) {
#if LIBAVCODEC_VERSION_MAJOR >= 59 
                    const
#endif
                    AVCodec *try_codec;
                    avcodec_free_context(&dp.codec_ctx);
                    dp.codec_ctx = nullptr;

                    // For AAC files, try the Fraunhofer decoder if the native FFmpeg decoder failed
                    if (dp.codec->id == AV_CODEC_ID_AAC) {
                        try_codec = avcodec_find_decoder_by_name("libfdk_aac");
                        if (try_codec) {
                            dp.codec = try_codec;
                            repeat = true;
                            continue;
                        }
                    }
                }
                if (!multithread)
                    output_fferror(rc, "Could not open codec");
                return ScanReturn::ERR;
            }
            repeat = false;
        } while (repeat);
        codec_id = dp.codec->id;
#if OLD_CHANNEL_LAYOUT
        dp.nb_channels = dp.codec_ctx->channels;
#else
        dp.nb_channels = dp.codec_ctx->ch_layout.nb_channels;
#endif

        // Display some information about the file
        if (output_progress)
            output_ok("Stream #{}: {}, {}{:L} Hz, {} ch",
                dp.stream_id,
                dp.codec->long_name,
                dp.codec_ctx->bits_per_raw_sample > 0 ? rsgain::format("{} bit, ", dp.codec_ctx->bits_per_raw_sample) : "",
                dp.codec_ctx->sample_rate,
                dp.nb_channels
            );

        // Only initialize swresample if we need to convert the format
        dp.output_format = determine_output_format(dp.codec_ctx->sample_fmt);
        if (dp.codec_ctx->sample_fmt != dp.output_format) {
#if OLD_CHANNEL_LAYOUT
            if (!dp.codec_ctx->channel_layout)
                dp.codec_ctx->channel_layout = av_get_default_channel_layout(dp.codec_ctx->channels);
            dp.swr = swr_alloc_set_opts(nullptr,
                    dp.codec_ctx->channel_layout,
                    dp.output_format,
                    dp.codec_ctx->sample_rate,
                    dp.codec_ctx->channel_layout,
                    dp.codec_ctx->sample_fmt,
                    dp.codec_ctx->sample_rate,
                    0,
                    nullptr
                );
#else
            swr_alloc_set_opts2(&dp.swr,
                &dp.codec_ctx->ch_layout,
                dp.output_format,
                dp.codec_ctx->sample_rate,
                &dp.codec_ctx->ch_layout,
                dp.codec_ctx->sample_fmt,
                dp.codec_ctx->sample_rate,
                0,
                nullptr
            );
#endif
            if (!dp.swr) {
                if (!multithread)
                    output_error("Could not allocate libswresample context");
                return ScanReturn::ERR;
            }

            rc = swr_init(dp.swr);
            if (rc < 0) {
                if (!multithread)
                    output_fferror(rc, "Could not open libswresample context");
                return ScanReturn::ERR;
            }
        }
    }

    // Initialize libebur128
    peak_mode = config.true_peak ? EBUR128_MODE_TRUE_PEAK : EBUR128_MODE_SAMPLE_PEAK;
    ebur128 = std::unique_ptr<ebur128_state, Ebur128Deleter>(ebur128_init((unsigned int) dp.nb_channels,
        static_cast<size_t>(dp.codec_ctx->sample_rate),
        EBUR128_MODE_I | peak_mode
    ));
    if (!ebur128) {
        if (!multithread)
            output_error("Could not initialize libebur128 scanner");
        return ScanReturn::ERR;
    }
    if (dp.nb_channels == 1 && config.dual_mono)
        ebur128_set_channel(ebur128.get(), 0, EBUR128_DUAL_MONO);

    if (output_progress) { 
        double duration;
        if (dp.stream->duration != AV_NOPTS_VALUE)
            duration = dp.stream->duration * dp.time_base;
        else if (dp.format_ctx->duration != AV_NOPTS_VALUE)
            duration = static_cast<double>(dp.format_ctx->duration) * (1.0 / static_cast<double>(AV_TIME_BASE));
        else
            output_progress = false;
        if (output_progress) {
            int start = 0;
            if (dp.stream->start_time != AV_NOPTS_VALUE)
                start = (int) std::round((double) dp.stream->start_time * dp.time_base);
            progress_bar = std::make_unique<ProgressBar>(start, (int) std::round(duration));
        }
    }

    if (dp.output_format == AV_SAMPLE_FMT_S16)
        return scan_loop<short, ebur128_add_frames_short>(dp, progress_bar.get());
    else if (dp.output_format == AV_SAMPLE_FMT_FLT)
        return scan_loop<float, ebur128_add_frames_float>(dp, progress_bar.get());
    else if (dp.output_format == AV_SAMPLE_FMT_S32)
        return scan_loop<int, ebur128_add_frames_int>(dp, progress_bar.get());
    else if (dp.output_format == AV_SAMPLE_FMT_DBL)
        return scan_loop<double, ebur128_add_frames_double>(dp, progress_bar.get());
    else
        return ScanReturn::ERR;
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
            error |= !tag_track(track, config);

        if (tab_output) {
            // Filename;Loudness;Gain (dB);Peak;Peak (dB);Peak Type;Clipping Adjustment;
            rsgain::print(stream, "{}\t", track.path.filename().string());
            track.result.track_loudness == -HUGE_VAL ? rsgain::print(stream, "-∞\t") : rsgain::print(stream, "{:.2f}\t", track.result.track_loudness);
            rsgain::print(stream, "{:.2f}\t", track.result.track_gain);
            rsgain::print(stream, "{:.6f}\t", track.result.track_peak);
            track.result.track_peak == 0.0 ? rsgain::print(stream, "-∞\t") : rsgain::print(stream, "{:.2f}\t", 20.0 * log10(track.result.track_peak));
            rsgain::print(stream, "{}\t", config.true_peak ? "True" : "Sample");
            rsgain::print(stream, "{}\n", track.tclip ? "Y" : "N");
            if (config.do_album && ((size_t) (&track - &tracks[0]) == (nb_files - 1))) {
                rsgain::print(stream, "{}\t", "Album");
                track.result.album_loudness == -HUGE_VAL ? rsgain::print(stream, "-∞\t") : rsgain::print(stream, "{:.2f}\t", track.result.album_loudness);
                rsgain::print(stream, "{:.2f}\t", track.result.album_gain);
                rsgain::print(stream, "{:.6f}\t", track.result.album_peak);
                track.result.album_peak == 0.0 ? rsgain::print(stream, "-∞\t") : rsgain::print(stream, "{:.2f}\t", 20.0 * log10(track.result.album_peak));
                rsgain::print(stream, "{}\t", config.true_peak ? "True" : "Sample");
                rsgain::print(stream, "{}\n", track.aclip ? "Y" : "N");
            }
        } 
        
        // Human-readable output
        if (human_output) {
            rsgain::print("\nTrack: {}\n", track.path.string());
            rsgain::print("  Loudness: {} LUFS\n", track.result.track_loudness == -HUGE_VAL ? "   -∞" : rsgain::format("{:8.2f}", track.result.track_loudness));
            rsgain::print("  Peak:     {:8.6f} ({} dB)\n",
                track.result.track_peak,
                track.result.track_peak == 0.0 ? "-∞" : rsgain::format("{:.2f}", 20.0 * log10(track.result.track_peak))
            );
            rsgain::print("  Gain:     {:8.2f} dB {}{}\n", 
                track.result.track_gain,
                track.type == FileType::OPUS && (config.opus_mode == 'r' || config.opus_mode == 's') ? rsgain::format("({})", GAIN_TO_Q78(track.result.track_gain)) : "",
                track.tclip ? " (adjusted to prevent clipping)" : ""
            );

            if (config.do_album && ((size_t) (&track - &tracks[0]) == (nb_files - 1))) {
                rsgain::print("\nAlbum:\n");
                rsgain::print("  Loudness: {} LUFS\n", track.result.album_loudness == -HUGE_VAL ? "   -∞" : rsgain::format("{:8.2f}", track.result.album_loudness));
                rsgain::print("  Peak:     {:8.6f} ({} dB)\n",
                    track.result.album_peak,
                    track.result.album_peak == 0.0 ? "-∞" : rsgain::format("{:.2f}", 20.0 * log10(track.result.album_peak))
                );
                rsgain::print("  Gain:     {:8.2f} dB {}{}\n", 
                    track.result.album_gain,
                    type == FileType::OPUS && (config.opus_mode == 'r' || config.opus_mode == 's') ? rsgain::format("({})", GAIN_TO_Q78(track.result.album_gain)) : "",
                    track.aclip ? " (adjusted to prevent clipping)" : ""
                );
            }
            rsgain::print("\n");
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
            if (track.result.track_loudness != -HUGE_VAL)
                data.total_loudness += track.result.track_loudness;
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
    if (config.album_as_aes77) {
        album_loudness = -HUGE_VAL;
        album_peak = 0.0;
        for (const Track &track : tracks) {
            if (album_loudness < track.result.track_loudness) {
                album_loudness = track.result.track_loudness;
            }
            if (album_peak < track.result.track_peak) {
                album_peak = track.result.track_peak;
            }
        }
    }

    else {
        std::vector<ebur128_state*> states;
        states.reserve(tracks.size());
        for (const Track &track : tracks)
            if (track.result.track_loudness != -HUGE_VAL)
                states.emplace_back(track.ebur128.get());

        if (ebur128_loudness_global_multiple(states.data(), states.size(), &album_loudness) != EBUR128_SUCCESS)
            album_loudness = config.target_loudness;

        album_peak = std::max_element(tracks.begin(),
                         tracks.end(),
                         [](const auto &a, const auto &b) { return a.result.track_peak < b.result.track_peak; }
                     )->result.track_peak;
    }

    double album_gain = (type == FileType::OPUS && config.opus_mode == 's' ? -23.0 : config.target_loudness)
                         - album_loudness;
    for (Track &track : tracks) {
        track.result.album_gain = album_gain;
        track.result.album_peak = album_peak;
        track.result.album_loudness = album_loudness;
    }
}
