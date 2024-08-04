/*
 * Loudness normalizer based on the EBU R128 standard
 *
 * Copyright (c) 2014, Alessandro Ghedini
 * All rights reserved.
 *
 * rsgain by complexlogic, 2022
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <getopt.h>
#include <string>
#include <locale>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
}
#include <taglib/taglib.h>
#define HAS_TAGLIB2 TAGLIB_MAJOR_VERSION > 1
#if HAS_TAGLIB2
#include <taglib/tversionnumber.h>
#endif
#include <ebur128.h>

#include "rsgain.hpp"
#include "tag.hpp"
#include "config.h"
#include "scan.hpp"
#include "output.hpp"
#include "easymode.hpp"

#define PRINT_LIB(lib, version) rsgain::print("  " COLOR_YELLOW " {:<14}" COLOR_OFF " {}\n", lib, version)
#define PRINT_LIB_FFMPEG(name, fn) \
    ffver = fn(); \
    PRINT_LIB(name, rsgain::format("{}.{}.{}", AV_VERSION_MAJOR(ffver), AV_VERSION_MINOR(ffver), AV_VERSION_MICRO(ffver)))

#ifdef _WIN32
#include <windows.h>
void init_console();
void set_cursor_visibility(HANDLE console, BOOL setting, BOOL *previous);
#endif

static void help_main();
static void version();
static inline void help_custom();

int quiet = 0;

#ifdef _WIN32
BOOL initial_cursor_visibility;
static void init_console()
{
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleMode(console, ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    set_cursor_visibility(console, FALSE, &initial_cursor_visibility);
    ProgressBar::set_console(console);
}

// Make the console cursor invisible for the progress bar
static void set_cursor_visibility(HANDLE console, BOOL setting, BOOL* previous)
{
    CONSOLE_CURSOR_INFO info;
    GetConsoleCursorInfo(console, &info);
    if (previous)
        *previous = info.bVisible;
    info.bVisible = setting;
    SetConsoleCursorInfo(console, &info);
}
#endif

void quit(int status)
{
#ifdef _WIN32
    if (initial_cursor_visibility)
        set_cursor_visibility(GetStdHandle(STD_OUTPUT_HANDLE), TRUE, nullptr);
#endif
    exit(status);
}

bool parse_target_loudness(const char *value, double &target_loudness)
{
    int loudness = atoi(value);
    if (loudness < MIN_TARGET_LOUDNESS || loudness > MAX_TARGET_LOUDNESS) {
        output_error("Invalid target loudness value '{}'", value);
        return false;
    }
    target_loudness = (double) loudness;
    return true;  
}

bool parse_mode(const char *name, const char *valid_modes, const char *value, char &mode)
{
    const std::string_view vm = valid_modes;
    size_t pos = vm.find_first_of(*value);
    if (pos != std::string::npos) {
        mode = valid_modes[pos];
        return true;
    }
    output_error("Invalid {} mode: '{}'", name, value);
    return false;
}

bool parse_id3v2_version(const char *value, unsigned int &version)
{
    if (MATCH(value, "keep")) {
        version = ID3V2_KEEP;
        return true;
    }
    else {
        unsigned int id3v2version = (unsigned int) strtoul(value, nullptr, 10);
        if (!(id3v2version == 3) && !(id3v2version == 4)) {
            output_error("Invalid ID3v2 version '{}'; only 'keep', '3', and '4' are supported.", value);
            return false;
        }
        version = id3v2version;
        return true;
    }
}

bool parse_max_peak_level(const char *value, double &peak)
{
    char *rest = nullptr;
    double max_peak = strtod(value, &rest);
    if (rest == value || !isfinite(max_peak)) {
        output_error("Invalid max peak level '{}'", value);
        return false;
    }

    peak = max_peak;
    return true;
}

std::pair<bool, bool> parse_output_mode(const std::string_view arg)
{
    std::pair<bool, bool> ret(false, false);
    for (char c : arg) {
        if (c == 's')
            ret.first = true;
        else if (c == 'a')
            ret.second = true;
        else {
            output_fail("Unrecognized output argument '{}'", c);
            quit(EXIT_FAILURE);
        }
    }
    return ret;
}

// Parse Custom Mode command line arguments
static void custom_mode(int argc, char *argv[])
{
    int rc, i;
    unsigned int nb_files   = 0;
    opterr = 0;

    const char *short_opts = "+ac:m:tl:O::qps:LSI:o:h?";
    static struct option long_opts[] = {
        { "album",           no_argument,       nullptr, 'a' },
        { "skip-existing",   no_argument,       nullptr, 'S' },

        { "clip-mode",       required_argument, nullptr, 'c' },
        { "max-peak",        required_argument, nullptr, 'm' },
        { "true-peak",       no_argument,       nullptr, 't' },

        { "loudness",        required_argument, nullptr, 'l' },

        { "output",          optional_argument, nullptr, 'O' },
        { "quiet",           no_argument,       nullptr, 'q' },
        { "preserve-mtimes", no_argument,       nullptr, 'p' },

        { "tagmode",         required_argument, nullptr, 's' },
        { "lowercase",       no_argument,       nullptr, 'L' },
        { "id3v2-version",   required_argument, nullptr, 'I' },
        { "opus-mode",       required_argument, nullptr, 'o' },
        { "help",            no_argument,       nullptr, 'h' },
        { 0, 0, 0, 0 }
    };

    Config config = {
        .tag_mode = 's',
        .skip_existing = false,
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.0,
        .true_peak = false,
        .clip_mode = 'n',
        .do_album = false,
        .tab_output = OutputType::NONE,
        .sep_header = false,
        .sort_alphanum = false,
        .lowercase = false,
        .id3v2version = ID3V2_KEEP,
        .opus_mode = 'd',
        .skip_mp4 = false,
        .preserve_mtimes = false
    };

    while ((rc = getopt_long(argc, argv, short_opts, long_opts, &i)) != -1) {
        switch (rc) {
            case 'a':
                config.do_album = true;
                break;

            case 'S':
                config.skip_existing = true;
                break;

            case 'c':
                if (!parse_clip_mode(optarg, config.clip_mode))
                    quit(EXIT_FAILURE);
                break;

            case 'm': {
                if (!parse_max_peak_level(optarg, config.max_peak_level))
                    quit(EXIT_FAILURE);
                break;
            }

            case 't': {
                config.true_peak = true;
                break;
            }

            case 'l': {
                if (!parse_target_loudness(optarg, config.target_loudness))
                    quit(EXIT_FAILURE);
                break;
            }

            case 'O':
                config.tab_output = OutputType::STDOUT;
                if (optarg) {
                    const auto& [sep_header, sort_alphanum] = parse_output_mode(optarg);
                    config.sep_header = sep_header;
                    config.sort_alphanum = sort_alphanum;
                }
                quiet = 1;
                break;

            case 'q':
                quiet = 1;
                break;

            case 'p':
                config.preserve_mtimes = true;
                break;

            case 's': {
                if (!parse_tag_mode_custom(optarg, config.tag_mode))
                    quit(EXIT_FAILURE);
                break;
            }

            case 'L':
                config.lowercase = true;
                break;

            case 'I':
                if (!parse_id3v2_version(optarg, config.id3v2version))
                    quit(EXIT_FAILURE);
                break;

            case 'o':
                if (!parse_opus_mode(optarg, config.opus_mode))
                    quit(EXIT_FAILURE);
                break;
                
            case 'h':
                help_custom();
                quit(EXIT_SUCCESS);
                break;

            case '?':
                if (optopt)
                    output_fail("Unrecognized option '{:c}'", optopt);
                else
                    output_fail("Unrecognized option '{}'", argv[optind - 1] + 2);
                quit(EXIT_FAILURE);
        }
    }

    nb_files = (unsigned int) (argc - optind);
    if (!nb_files) {
        output_fail("No files were specified");
        quit(EXIT_FAILURE);
    }

    std::unique_ptr<ScanJob> job(ScanJob::factory(argv + optind, nb_files, config));
    if (!job) {
        output_fail("File list is not valid");
        quit(EXIT_FAILURE);
    }
    job->scan();
    if (job->error)
        quit(EXIT_FAILURE);
}

// Parse main arguments
int main(int argc, char *argv[]) {
    int rc, i;
    char *command = nullptr;
    opterr = 0;
    
    const char *short_opts = "+hv?";
    static struct option long_opts[] = {
        { "help",    no_argument, nullptr, 'h' },
        { "version", no_argument, nullptr, 'v' },
        { 0, 0, 0, 0 }
    };
    std::locale::global(std::locale(""));
    av_log_set_callback(nullptr);

#ifdef _WIN32
    init_console();
#endif 

    while ((rc = getopt_long(argc, argv, short_opts, long_opts, &i)) !=-1) {
        switch (rc) {
            case 'h':
                help_main();
                quit(EXIT_SUCCESS);
                break;

            case 'v':
                version();
                quit(EXIT_SUCCESS);
                break;

            case '?':
                if (optopt)
                    output_fail("Unrecognized option '{:c}'", optopt);
                else
                    output_fail("Unrecognized option '{}'", argv[optind - 1] + 2);
                quit(EXIT_FAILURE);
                break;
        }
    }

    if (argc == optind) {
        help_main();
        quit(EXIT_SUCCESS);
    }
    
    // Parse and run command
    command = argv[optind];
    char **subargs = argv + optind;
    int num_subargs = argc - optind;
    optind = 1;
    if (MATCH(command, "easy"))
        easy_mode(num_subargs, subargs);
    else if (MATCH(command, "custom"))
        custom_mode(num_subargs, subargs);
    else {
        output_fail("Invalid command '{}'", command);
        quit(EXIT_FAILURE);
    }
    quit(EXIT_SUCCESS);
}

static void help_main() {
    rsgain::print(COLOR_RED "Usage: " COLOR_OFF "{}{}{} [OPTIONS] <command> ...\n", COLOR_GREEN, EXECUTABLE_TITLE, COLOR_OFF);

    rsgain::print("{} {} supports writing tags to the following file types:\n", PROJECT_NAME, PROJECT_VERSION);
    rsgain::print("  FLAC (.flac), Ogg (.ogg, .oga, .spx), Opus (.opus), MP2 (.mp2),\n");
    rsgain::print("  MP3 (.mp3), MP4 (.mp4, .m4a), WMA (.wma), WavPack (.wv), APE (.ape),\n");
    rsgain::print("  WAV (.wav), AIFF (.aiff, .aif, .snd), and TAK (.tak).\n");

    rsgain::print("\n");
    rsgain::print(COLOR_RED "Options:\n" COLOR_OFF);

    CMD_HELP("--help",     "-h", "Show this help");
    CMD_HELP("--version",  "-v", "Show version number");
    
    rsgain::print("\n");
    rsgain::print(COLOR_RED "Commands:\n" COLOR_OFF);

    CMD_CMD("easy",     "Easy Mode:   Recursively scan a directory with recommended settings");
    CMD_CMD("custom",   "Custom Mode: Scan individual files with custom settings");
    rsgain::print("\n");
    rsgain::print("Run '{} easy --help' or '{} custom --help' for more information.", EXECUTABLE_TITLE, EXECUTABLE_TITLE);

    rsgain::print("\n\n");
    rsgain::print("Please report any issues to " PROJECT_URL "/issues\n\n");
}

static inline void help_custom() {
    rsgain::print(COLOR_RED "Usage: " COLOR_OFF "{}{}{} custom [OPTIONS] FILES...\n", COLOR_GREEN, EXECUTABLE_TITLE, COLOR_OFF);

    rsgain::print("  Custom Mode allows the user to specify the options to scan the files with. The\n");
    rsgain::print("  list of files to scan must be listed explicitly after the options.\n");
    rsgain::print("\n");
    
    rsgain::print(COLOR_RED "Options:\n" COLOR_OFF);
    CMD_HELP("--help",     "-h", "Show this help");
    rsgain::print("\n");

    CMD_HELP("--album",  "-a", "Calculate album gain and peak");
    CMD_HELP("--skip-existing", "-S", "Don't scan files with existing ReplayGain information");
    rsgain::print("\n");

    CMD_HELP("--tagmode=s", "-s s", "Scan files but don't write ReplayGain tags (default)");
    CMD_HELP("--tagmode=d", "-s d",  "Delete ReplayGain tags from files");
    CMD_HELP("--tagmode=i", "-s i",  "Scan and write ReplayGain 2.0 tags to files");
    rsgain::print("\n");

    CMD_HELP("--loudness=n",  "-l n",  "Use n LUFS as target loudness (" STR(MIN_TARGET_LOUDNESS) " ≤ n ≤ " STR(MAX_TARGET_LOUDNESS) ")");
    rsgain::print("\n");

    CMD_HELP("--clip-mode=n", "-c n", "No clipping protection (default)");
    CMD_HELP("--clip-mode=p", "-c p", "Clipping protection enabled for positive gain values only");
    CMD_HELP("--clip-mode=a", "-c a", "Clipping protection always enabled");
    CMD_HELP("--max-peak=n", "-m n", "Use max peak level n dB for clipping protection");
    CMD_HELP("--true-peak",  "-t", "Use true peak for peak calculations");

    rsgain::print("\n");

    CMD_HELP("--lowercase", "-L", "Write lowercase tags (MP2/MP3/MP4/WMA/WAV/AIFF)");
    CMD_CONT("This is non-standard but sometimes needed");
    CMD_HELP("--id3v2-version=keep", "-I keep", "Keep file's existing ID3v2 version, 3 if none exists (default)");
    CMD_HELP("--id3v2-version=3", "-I 3", "Write ID3v2.3 tags to MP2/MP3/WAV/AIFF");
    CMD_HELP("--id3v2-version=4", "-I 4", "Write ID3v2.4 tags to MP2/MP3/WAV/AIFF");

    rsgain::print("\n");

    CMD_HELP("--opus-mode=d", "-o d", "Write standard ReplayGain tags, clear header output gain (default)");
    CMD_HELP("--opus-mode=r", "-o r", "Write R128_*_GAIN tags, clear header output gain");
    CMD_HELP("--opus-mode=s", "-o s", "Same as 'r', plus override target loudness to -23 LUFS");
    CMD_HELP("--opus-mode=t", "-o t", "Write track gain to header output gain");
    CMD_HELP("--opus-mode=a", "-o a", "Write album gain to header output gain");

    rsgain::print("\n");

    CMD_HELP("--output", "-O",  "Output tab-delimited scan data to stdout");
    CMD_HELP("--output=s", "-O s",  "Output with sep header (needed for Microsoft Excel compatibility)");
    CMD_HELP("--output=a", "-O a",  "Output with files sorted in alphanumeric order");

    CMD_HELP("--preserve-mtimes", "-p", "Preserve file mtimes");
    CMD_HELP("--quiet",      "-q",  "Don't print scanning status messages");

    rsgain::print("\n");

    rsgain::print("Please report any issues to " PROJECT_URL "/issues\n");
    rsgain::print("\n");
}

static void version() {
    unsigned int ffver;
    int ebur128_v_major = 0;
    int ebur128_v_minor = 0;
    int ebur128_v_patch = 0;
    rsgain::print(COLOR_GREEN PROJECT_NAME COLOR_OFF " " PROJECT_VERSION
#if defined COMMITS_SINCE_TAG && defined COMMIT_HASH
                "-r" COMMITS_SINCE_TAG "-" COMMIT_HASH
#endif
                " - using:\n"
    );

    // Library versions
    ebur128_get_version(&ebur128_v_major, &ebur128_v_minor, &ebur128_v_patch);
    PRINT_LIB("libebur128", rsgain::format("{}.{}.{}", ebur128_v_major, ebur128_v_minor, ebur128_v_patch));
    PRINT_LIB_FFMPEG("libavformat", avformat_version);
    PRINT_LIB_FFMPEG("libavcodec", avcodec_version);
    PRINT_LIB_FFMPEG("libavutil", avutil_version);
    PRINT_LIB_FFMPEG("libswresample", swresample_version);
#if HAS_TAGLIB2
    TagLib::VersionNumber tver = TagLib::runtimeVersion();
    PRINT_LIB("TagLib", rsgain::format("{}.{}{}", tver.majorVersion(), tver.minorVersion(), tver.patchVersion() ? rsgain::format(".{}", tver.patchVersion()) : ""));
#else
    rsgain::print("\n");
    rsgain::print("Built with:\n");
    PRINT_LIB("TagLib", rsgain::format("{}.{}{}", TAGLIB_MAJOR_VERSION, TAGLIB_MINOR_VERSION, TAGLIB_PATCH_VERSION ? rsgain::format(".{}", TAGLIB_PATCH_VERSION) : ""));
#endif
    rsgain::print("\n");

#if defined(__GNUC__) && !defined(__clang__)
    rsgain::print(COLOR_YELLOW "{:<17}" COLOR_OFF " GCC {}.{}\n", "Compiler:", __GNUC__, __GNUC_MINOR__);
#endif

#ifdef __clang__
    rsgain::print(COLOR_YELLOW "{:<17}" COLOR_OFF " "
#ifdef __apple_build_version__
    "Apple "
#endif
    "Clang {}.{}.{}\n", "Compiler:", __clang_major__, __clang_minor__, __clang_patchlevel__);
#endif

#ifdef _MSC_VER
    rsgain::print(COLOR_YELLOW "{:<17}" COLOR_OFF " Microsoft C/C++ {:.2f}\n", "Compiler:", (float) _MSC_VER / 100.0f);
#endif

    rsgain::print(COLOR_YELLOW "{:<17}" COLOR_OFF " " BUILD_DATE "\n", "Build Date:");
}
