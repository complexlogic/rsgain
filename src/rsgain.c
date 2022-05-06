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
#include <stdbool.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <getopt.h>

#include <ebur128.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>

#include "rsgain.h"
#include "tag.h"
#include "config.h"
#include "scan.h"
#include "output.h"
#include "easymode.h"

#ifdef _WIN32
#include <windows.h>
void init_console(void);
void set_cursor_visibility(BOOL setting, BOOL *previous);
#endif

static void help_main(void);
static void version(void);
static inline void help_custom(void);
static inline void help_easy(void);

// Global variables
int multithread = 0;

#ifdef _WIN32
HANDLE console;
BOOL initial_cursor_visibility;
#endif

#ifdef _WIN32
 static void init_console()
{
    SetConsoleCP(CP_UTF8);
    console = CreateFileA(
        "CONOUT$",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        0
    );
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleMode(console, ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    set_cursor_visibility(FALSE, &initial_cursor_visibility);
}

// Make the console cursor invisible for the progress bar
static void set_cursor_visibility(BOOL setting, BOOL *previous)
{
    CONSOLE_CURSOR_INFO info;
    GetConsoleCursorInfo(console, &info);
    if (previous != NULL) {
        *previous = info.bVisible;
    }
    info.bVisible = setting;
    SetConsoleCursorInfo(console, &info);
}
#endif

void quit(int status)
{
    #ifdef _WIN32
    if (initial_cursor_visibility) {
        set_cursor_visibility(TRUE, NULL);
    }
    #endif
    exit(status);
}

void parse_pregain(const char *value, Config *config)
{
    char *rest = NULL;
    config->pre_gain = strtod(value, &rest);

    if (!rest ||
        (rest == value) ||
        !isfinite(config->pre_gain))
        output_fail("Invalid pregain value '%s' (dB/LU)", value);
}

void parse_mode(const char *value, Config *config)
{
    // for mp3gain compatibilty, include modes that do nothing
    char *valid_modes = "cdielavsr";
    config->mode = value[0];
    if (strchr(valid_modes, config->mode) == NULL)
        output_fail("Invalid tag mode: '%s'", value);
    if (config->mode == 'l') {
        config->unit = UNIT_LU;
    }
}

void parse_id3v2version(const char *value, Config *config)
{
    config->id3v2version = atoi(value);
    if (!(config->id3v2version == 3) && !(config->id3v2version == 4))
        output_fail("Invalid ID3v2 version '%s'; only 3 and 4 are supported.", value);
}

void parse_max_true_peak_level(const char *value, Config *config)
{
    // new style, argument in dBTP, sets max. true peak level
    config->no_clip = true;

    char *rest = NULL;
    config->max_true_peak_level = strtod(value, &rest);

    if (!rest ||
        (rest == value) ||
        !isfinite(config->pre_gain))
        output_fail("Invalid max. true peak level '%s' (dBTP)", value);
}

// Parse Easy Mode command line arguments
static void easy_mode(int argc, char *argv[])
{
    int rc, i;
    char *overrides_file = NULL;
    const char *short_opts = "+hqm:o:";
    static struct option long_opts[] = {
        { "help",         no_argument,       NULL, 'h' },
        { "quiet",        no_argument,       NULL, 'q' },

        { "multithread",  required_argument, NULL, 'm' },
        { 0, 0, 0, 0 }
    };
    while ((rc = getopt_long(argc, argv, short_opts, long_opts, &i)) !=-1) {
        switch (rc) {
            case 'h':
                help_easy();
                quit(EXIT_SUCCESS);
                break;

            case 'q':
                quiet = true;
                break;
            
            case 'm':
                multithread = atoi(optarg);
                if (multithread < 2)
                    multithread = 0;
                break;
            
            case 'o':
                overrides_file = optarg;
                break;
        }
    }

    if (argc == optind) {
        output("Error: You must specific the directory to scan\n");
        quit(EXIT_FAILURE);
    }

    scan_easy(argv[optind], overrides_file);
}

// Parse Custom Mode command line arguments
static void custom_mode(int argc, char *argv[])
{
    int rc, i;
    unsigned nb_files   = 0;

    const char *short_opts = "+rackK:d:Oqs:LSI:h?";
    static struct option long_opts[] = {
        { "track",        no_argument,       NULL, 'r' },
        { "album",        no_argument,       NULL, 'a' },

        { "clip",         no_argument,       NULL, 'c' },
        { "noclip",       no_argument,       NULL, 'k' },
        { "maxtpl",       required_argument, NULL, 'K' },

        { "pregain",      required_argument, NULL, 'd' },

        { "output",       no_argument,       NULL, 'O' },
        { "quiet",        no_argument,       NULL, 'q' },

        { "tagmode",      required_argument, NULL, 's' },
        { "lowercase",    no_argument,       NULL, 'L' },
        { "striptags",    no_argument,       NULL, 'S' },
        { "id3v2version", required_argument, NULL, 'I' },

        { "help",         no_argument,       NULL, 'h' },
        { 0, 0, 0, 0 }
    };

    Config config = {
        .mode = 's',
        .unit = UNIT_DB,
        .pre_gain = 0.0f,
        .max_true_peak_level = 0.f,
        .no_clip = false,
        .warn_clip = true,
        .do_album = false,
        .tab_output = false,
        .lowercase = false,
        .strip = false,
        .id3v2version = 4
    };

    while ((rc = getopt_long(argc, argv, short_opts, long_opts, &i)) !=-1) {
        switch (rc) {
            case 'r':
                /* noop */
                break;

            case 'a':
                config.do_album = true;
                break;

            case 'c':
                config.warn_clip = false;
                break;

            case 'k':
                // old-style, no argument, now defaults to -1 dBTP max. true peak level
                config.no_clip = true;
                break;

            case 'K': {
                parse_max_true_peak_level(optarg, &config);
                break;
            }

            case 'd': {
                parse_pregain(optarg, &config);
                break;
            }

            case 'O':
                config.tab_output = true;
                break;

            case 'q':
                quiet = 1;
                break;

            case 's': {
                parse_mode(optarg, &config);
                break;
            }

            case 'L':
                config.lowercase = true;
                break;

            case 'S':
                config.strip = true;
                break;

            case 'I':
                parse_id3v2version(optarg, &config);
                break;

            case '?':
                if (optopt == 0) {
                    // actual option '-?'
                    help_custom();
                    quit(EXIT_SUCCESS);
                } else {
                    // getopt error, message already printed
                    quit(EXIT_FAILURE);	// error
                }
            case 'h':
                help_custom();
                quit(EXIT_SUCCESS);
                break;
        }
    }

    nb_files = argc - optind;
    if (!nb_files) {
        output("Error: No files specified\n");
        quit(EXIT_FAILURE);
    }
    scan(nb_files, argv + optind, &config);
}

// Parse main arguments
int main(int argc, char *argv[]) {
    int rc, i;
    char *command = NULL;
    
    const char *short_opts = "+hv";
    static struct option long_opts[] = {
        { "help",         no_argument,       NULL, 'h' },
        { "version",      no_argument,       NULL, 'v' },
        { 0, 0, 0, 0 }
    };
  
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
    if (!strcmp(command, "easy")) {
        easy_mode(num_subargs, subargs);
    }
    else if (!strcmp(command, "custom")) {
        custom_mode(num_subargs, subargs);
    }
    else {
        output("Error: Unrecognized command \"%s\"\n", command);
        quit(EXIT_FAILURE);
    }
    quit(EXIT_SUCCESS);
}


static void help_main(void) {

    output(COLOR_RED "Usage: " COLOR_OFF "%s%s%s [OPTIONS] <command> ...\n", COLOR_GREEN, EXECUTABLE_TITLE, COLOR_OFF);

    output("%s %s supports writing tags to the following file types:\n", PROJECT_NAME, PROJECT_VERSION);
    output("  FLAC (.flac), Ogg (.ogg, .oga, .spx), OPUS (.opus), MP2 (.mp2),\n");
    output("  MP3 (.mp3), MP4 (.m4a), WMA (.wma), WavPack (.wv), APE (.ape).\n");
    output("  Experimental: WAV (.wav), AIFF (.aiff, .aif, .snd).\n");

    output("\n");
    output(COLOR_RED "Options:\n" COLOR_OFF);

    CMD_HELP("--help",     "-h", "Show this help");
    CMD_HELP("--version",  "-v", "Show version number");
    
    output("\n");
    output(COLOR_RED "Commands:\n" COLOR_OFF);

    CMD_CMD("easy",     "Easy Mode:   Recursively scan a directory with recommended settings");
    CMD_CMD("custom",   "Custom Mode: Scan individual files with custom settings");
    output("\n");
    output("Run '%s easy --help' or '%s custom --help' for more information.", EXECUTABLE_TITLE, EXECUTABLE_TITLE);

    output("\n\n");
    output("Please report any issues to " PROJECT_URL "/issues\n\n");
}


static inline void help_easy(void) {

    output(COLOR_RED "Usage: " COLOR_OFF "%s%s%s easy [OPTIONS] DIRECTORY\n", COLOR_GREEN, EXECUTABLE_TITLE, COLOR_OFF);

    output("  Easy Mode recursively scans a directory using the recommended settings for each\n");
    output("  file type. Easy Mode assumes that you have your music library organized with each album\n");
    output("  in its own folder.\n");

    output("\n");
    output(COLOR_RED "Options:\n" COLOR_OFF);

    CMD_HELP("--help",     "-h", "Show this help");
    CMD_HELP("--quiet",      "-q",  "Don't print scanning status messages");
    output("\n");
    CMD_HELP("--multithread=n", "-m n", "Scan files with n parallel threads");
    CMD_HELP_SHORT("", "-o <path>", "Load override settings from file");

    output("\n");

    output("Please report any issues to " PROJECT_URL "/issues\n");
    output("\n");
}


static inline void help_custom(void) {

    output(COLOR_RED "Usage: " COLOR_OFF "%s%s%s custom [OPTIONS] FILES...\n", COLOR_GREEN, EXECUTABLE_TITLE, COLOR_OFF);

    output("  Custom Mode allows the user to specify the options to scan the files with. The\n");
    output("  list of files to scan must be listed explicitly after the options.\n");

    output("\n");
    output(COLOR_RED "Options:\n" COLOR_OFF);

    CMD_HELP("--help",     "-h", "Show this help");

    output("\n");

    CMD_HELP("--track",  "-r", "Calculate track gain only (default)");
    CMD_HELP("--album",  "-a", "Calculate album gain (and track gain)");

    output("\n");

    CMD_HELP("--clip",   "-c", "Ignore clipping warning");
    CMD_HELP("--noclip", "-k", "Lower track/album gain to avoid clipping (<= -1 dBTP)");
    CMD_HELP("--maxtpl=n", "-K n", "Avoid clipping; max. true peak level = n dBTP");

    CMD_HELP("--pregain=n",  "-d n",  "Apply n dB/LU pre-gain value (-5 for -23 LUFS target)");

    output("\n");

    CMD_HELP("--tagmode=d", "-s d",  "Delete ReplayGain tags from files");
    CMD_HELP("--tagmode=i", "-s i",  "Write ReplayGain 2.0 tags to files");
    CMD_HELP("--tagmode=e", "-s e",  "like '-s i', plus extra tags (reference, ranges)");
    CMD_HELP("--tagmode=l", "-s l",  "like '-s e', but LU units instead of dB");

    CMD_HELP("--tagmode=s", "-s s",  "Don't write ReplayGain tags (default)");

    output("\n");

    CMD_HELP("--lowercase", "-L", "Force lowercase tags (MP2/MP3/MP4/WMA/WAV/AIFF)");
    CMD_CONT("This is non-standard but sometimes needed");
    CMD_HELP("--striptags", "-S", "Strip tag types other than ID3v2 from MP2/MP3");
    CMD_CONT("Strip tag types other than APEv2 from WavPack/APE");
    CMD_HELP("--id3v2version=3", "-I 3", "Write ID3v2.3 tags to MP2/MP3/WAV/AIFF");
    CMD_HELP("--id3v2version=4", "-I 4", "Write ID3v2.4 tags to MP2/MP3/WAV/AIFF (default)");

    output("\n");

    CMD_HELP("--output", "-O",  "Database-friendly tab-delimited list output");
    CMD_HELP("--quiet",      "-q",  "Don't print scanning status messages");

    output("\n");

    output("Please report any issues to " PROJECT_URL "/issues\n");
    output("\n");
}


static void version(void) {
    int  ebur128_v_major     = 0;
    int  ebur128_v_minor     = 0;
    int  ebur128_v_patch     = 0;
    unsigned swr_ver         = 0;
    unsigned lavf_ver        = 0;
    unsigned lavc_ver        = 0;
    unsigned lavu_ver        = 0;
    char ebur128_version[15];
    char swr_version[15];
    char lavf_version[15];
    char lavc_version[15];
    char lavu_version[15];
    char taglib_version[15];

    // libebur128 version check
    ebur128_get_version(&ebur128_v_major, &ebur128_v_minor, &ebur128_v_patch);
    snprintf(ebur128_version, sizeof(ebur128_version), "%d.%d.%d",
        ebur128_v_major, ebur128_v_minor, ebur128_v_patch);

    // libavformat version
    lavf_ver = avformat_version();
    snprintf(lavf_version, sizeof(lavf_version), "%u.%u.%u",
        lavf_ver>>16, lavf_ver>>8&0xff, lavf_ver&0xff);

    // libavcodec version
    lavc_ver = avcodec_version();
    snprintf(lavc_version, sizeof(lavc_version), "%u.%u.%u",
        lavc_ver>>16, lavc_ver>>8&0xff, lavc_ver&0xff);

    // libavcodec version
    lavu_ver = avutil_version();
    snprintf(lavu_version, sizeof(lavu_version), "%u.%u.%u",
        lavu_ver>>16, lavu_ver>>8&0xff, lavu_ver&0xff);

    // libswresample version
    swr_ver = swresample_version();
    snprintf(swr_version, sizeof(swr_version), "%u.%u.%u",
        swr_ver>>16, swr_ver>>8&0xff, swr_ver&0xff);

    // taglib version
    taglib_get_version(taglib_version, sizeof(taglib_version));

    // Print versions
    output(COLOR_GREEN PROJECT_NAME COLOR_OFF " " PROJECT_VERSION " - using:\n");
    PRINT_LIB("libebur128", ebur128_version);
    PRINT_LIB("libavformat", lavf_version);
    PRINT_LIB("libavcodec", lavc_version);
    PRINT_LIB("libavutil", lavu_version);
    PRINT_LIB("libswresample", swr_version);
    output("\n");
    output("Built with:\n");
    PRINT_LIB("taglib", taglib_version);
}