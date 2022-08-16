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
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <cmath>

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

#include <fmt/core.h>
#include "output.hpp"
#include "config.h"

#ifdef _WIN32
#define print_buffer(buffer, length, stream) WriteConsoleA(console, buffer, length, NULL, NULL)
#else
#define print_buffer(buffer, length, stream) fputs(buffer, stream); fflush(stream)
#endif

int quiet = 0;
int disable_progress_bar = 0;
extern int multithread;

static void get_screen_size(unsigned *w, unsigned *h);
void quit(int status);

void progress_bar(unsigned ctrl, unsigned long x, unsigned long n, unsigned w) {
	int i;
	static int show_bar = 0;
	int c;
	static int prev_c = -1;
	static char *buffer = NULL;
	static double ratio;
	
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
				buffer = new char[w + 3];
			}
			
			ratio = x / (double) n;
			c     = ratio * w;
			
			// Only update if the progress bar has incremented
			if (c != prev_c) {
				fmt::print(" {:3.0f}% [", ratio * 100);
				memset(buffer, '=', c);
				memset(buffer + c, ' ', w - c);
				buffer[w] = ']';
				buffer[w + 1] = '\r';
				buffer[w + 2] = '\0';
				print_buffer(buffer, w + 2, stdout);
				prev_c = c;
			}
			break;

		case 2: /* end */
			if (show_bar == 1) {
				print_buffer("\n", 1, stdout);
				delete buffer;
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
