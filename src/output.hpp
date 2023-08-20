#pragma once

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

#ifdef __unix__
#include <unistd.h>
#include <syslog.h>
#include <limits.h>
#include <sys/ioctl.h>
#endif

#ifdef __APPLE__
#include <util.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include <string>
#include <string_view>
#include <fmt/core.h>

#define COLOR_GREEN	"[1;32m"
#define COLOR_YELLOW	"[1;33m"
#define COLOR_RED	"[1;31m"
#define COLOR_BGRED	"[1;41m"
#define COLOR_OFF	"[0m"

// The default Windows console font doesn't support the ✔ and ✘ characters
// and make it optional by defining NOUCHECKMARKS
#if defined _WIN32 || defined NOUCHECKMARKS
#define OK_CHAR "OK"
#define ERROR_CHAR "ERROR"
#define FAIL_CHAR "FAILURE"
#else
#define OK_CHAR "✔"
#define ERROR_CHAR "✘"
#define FAIL_CHAR "✘"
#endif
#define WARN_CHAR "!"

#define OK_PREFIX "[" COLOR_GREEN OK_CHAR COLOR_OFF "] "
#define WARN_PREFIX "[" COLOR_YELLOW WARN_CHAR COLOR_OFF "] "
#define ERROR_PREFIX "[" COLOR_RED ERROR_CHAR COLOR_OFF "] "
#define FAIL_PREFIX "[" COLOR_RED FAIL_CHAR COLOR_OFF "] "

extern int quiet;
#define output_ok(format, ...)    if (!quiet) fmt::print(OK_PREFIX format "\n" __VA_OPT__(,) __VA_ARGS__)
#define output_warn(format, ...)  if (!quiet) fmt::print(WARN_PREFIX format "\n" __VA_OPT__(,) __VA_ARGS__)
#define output_error(format, ...) fmt::print(stderr, ERROR_PREFIX format "\n" __VA_OPT__(,) __VA_ARGS__)
#define output_fail(format, ...)  fmt::print(stderr, FAIL_PREFIX format "\n" __VA_OPT__(,) __VA_ARGS__)

class ProgressBar {
    private:
        int c_prev = -1;
        int w_prev = -1;
        int pos_prev = -1;
        int start;
        int len = 0;
        char *buffer = nullptr;

#ifdef _WIN32
        inline static CONSOLE_SCREEN_BUFFER_INFO info;
        inline static HANDLE console;
#else
        inline static struct winsize ws;
#endif

    public:
        static int get_console_width();
        void begin(int start, int len);
        void update(int pos);
        void complete();
#ifdef _WIN32
        static void set_console(HANDLE c) { console = c; }
#endif
};

class MTProgress {
    private:
        size_t total;
        int cur = 0;

        int utf8_length(std::string_view string);
    
    public:
        MTProgress(size_t total) : total(total) {}
        void update(const std::string &path);
};
