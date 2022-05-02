/*
 * Loudness normalizer based on the EBU R128 standard
 *
 * Copyright (c) 2014, Alessandro Ghedini
 * All rights reserved.
 * 
 * Windows port by complexlogic, 2022
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
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#define LINE_MAX 2048
extern HANDLE console;
#else
#include <unistd.h>
#include <syslog.h>
#include <limits.h>
#include <sys/ioctl.h>
#endif

#include "output.h"
#include "config.h"

struct output_info {
	const char *prefix;
	int prefix_len;
	FILE *stream;
	bool append_newline;
};

int quiet = 0;
int disable_progress_bar = 0;
extern int multithread;

static void get_screen_size(unsigned *w, unsigned *h);
static void print_buffer(const char *buffer, int length, FILE *stream);
static void print_output(struct output_info *info, const char *fmt, va_list args);
void quit(int status);


void output(const char *fmt, ...)
{
	if (quiet) return;
	static struct output_info info = {
		NULL,
		0,
		NULL,
		false
	};
	info.stream = stdout;

	va_list args;
	va_start(args, fmt);
	print_output(&info, fmt, args);
	va_end(args);
}

void output_ok(const char *fmt, ...)
{
	if (quiet) return;
	static struct output_info info = {
		OK_PREFIX,
		LEN(OK_PREFIX),
		NULL,
		true
	};
	info.stream = stdout;

	va_list args;
	va_start(args, fmt);
	print_output(&info, fmt, args);
	va_end(args);
}

void output_warn(const char *fmt, ...)
{
	if (quiet) return;
	static struct output_info info = {
		WARN_PREFIX,
		LEN(WARN_PREFIX),
		NULL,
		true
	};
	info.stream = stdout;

	va_list args;
	va_start(args, fmt);
	print_output(&info, fmt, args);
	va_end(args);
}

void output_error(const char *fmt, ...)
{
	static struct output_info info = {
		ERROR_PREFIX,
		LEN(ERROR_PREFIX),
		NULL,
		true
	};
	info.stream = stderr;

	va_list args;
	va_start(args, fmt);
	print_output(&info, fmt, args);
	va_end(args);
}

void output_fail(const char *fmt, ...)
{
	static struct output_info info = {
		FAIL_PREFIX,
		LEN(FAIL_PREFIX),
		NULL,
		true
	};
	info.stream = stderr;

	va_list args;
	va_start(args, fmt);
	print_output(&info, fmt, args);
	va_end(args);
}

static void print_output(struct output_info *info, const char *fmt, va_list args)
{
	static char message[LINE_MAX];
	int newline = info->append_newline ? 1 : 0;
	int bytes = sizeof(message) - newline; // Allow for newline character
	char *p = message;

	// Copy prefix
	if (info->prefix != NULL && info->prefix_len < bytes) {
		strncpy(p, info->prefix, bytes);
		bytes -= info->prefix_len;
		p += info->prefix_len;
	}

	// Format the message
	if (bytes > 0) {
		int n = vsnprintf(p, bytes, fmt, args);
		if (n > 0 && n < bytes) {

			// Append newline to the end
			if (newline) {
				message[info->prefix_len + n] = '\n';
				message[info->prefix_len + n + 1] = '\0';
			}

			// Output message to console
			print_buffer(message, info->prefix_len + n + newline, info->stream);
		}
	}
}

static void print_buffer(const char *buffer, int length, FILE *stream)
{
	#ifdef _WIN32
	WriteConsoleA(console, buffer, length, NULL, NULL);
    #else
	fputs(buffer, stream);
    #endif
}

void progress_bar(unsigned ctrl, unsigned long x, unsigned long n, unsigned w) {
	int i;
	static int show_bar = 0;
	int c;
	static int prev_c = -1;
	static char *buffer = NULL;
	
	switch (ctrl) {
		case 0: /* init */
			if (quiet || multithread)
				break;
#ifndef _WIN32
			if (!isatty(fileno(stdout)))
				break;
#endif

			show_bar = 1;

			break;

		case 1: /* draw */
			if (show_bar != 1)
				return;

			if ((x != n) && (x % (n / 100 + 1) != 0))
				return;

			if (w == 0) {
				get_screen_size(&w, NULL);
				w -= 8;
				buffer = malloc(w + 3);
			}

			double ratio = x / (double) n;
			c     = ratio * w;
			
			// Only update if the progress bar has incremented
			if (c != prev_c) {
				output(" %3.0f%% [", ratio * 100);
				for (i = 0; i < c; i++)
					buffer[i] = '=';

				for (i = c; i < w; i++)
					buffer[i] = ' ';

				buffer[w] = ']';
				buffer[w + 1] = '\r';
				buffer[w + 2] = '\0';
				print_buffer(buffer, w + 2, stdout);
				#ifndef _WIN32
				fflush(stdout);
				#endif
				prev_c = c;
			}
			break;

		case 2: /* end */
			if (show_bar == 1) {
				print_buffer("\n", 1, stdout);
				free(buffer);
				buffer = NULL;
			}
			break;
	}
}

static void get_screen_size(unsigned *w, unsigned *h) {
  
	#ifdef _WIN32
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(console, &info);
	if (w != NULL) {
		*w = info.srWindow.Right - info.srWindow.Left + 1;
	}
	if (h != NULL) {
		*h = info.srWindow.Bottom - info.srWindow.Top + 1;
	}
  
	#else
	struct winsize ws;
	if (ioctl(fileno(stdout), TIOCGWINSZ, &ws) < 0 || !ws.ws_row || !ws.ws_col)
		return;

	if (w != NULL)
		*w = ws.ws_col;

	if (h != NULL)
		*h = ws.ws_row;
	#endif
}
