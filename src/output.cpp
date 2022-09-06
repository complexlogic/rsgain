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

void ProgressBar::begin(int start, int len)
{
	this->start = start;
	this->len = len;
	w_prev = -1;
	c_prev = -1;
	pos_prev = -1;
}

void ProgressBar::update(int pos)
{
	int w, c;
	if (pos == pos_prev)
		return;

	w = this->get_console_width() - 8;
	if (w <= 0)
		return;
	if (w != w_prev) {
		delete buffer;
		buffer = new char[w + 3];
	}

	float percent = ((float) pos / (float) len);
	c = (int) (percent * (float) w);

	// Only output if we've actually made progess since last the call, or the console width changed
	if (c != c_prev || w != w_prev) {
		fmt::print(" {:3.0f}% [", percent * 100.f);
		int i;
		for (i = 0; i < c; i++)
			buffer[i] = '=';
		
		for (i; i < w; i++)
			buffer[i] = ' ';

		buffer[w] = ']';
		buffer[w + 1] = '\r';
		buffer[w + 2] = '\0';
		print_buffer(buffer, w + 2, stdout);
	}

	c_prev = c;
	w_prev = w;
	pos_prev = pos;
}

void ProgressBar::complete()
{
	if (c_prev != w_prev)
		this->update(len);

	delete buffer;
	buffer = NULL;
	fmt::print("\n");
}

inline int ProgressBar::get_console_width()
{
#ifdef _WIN32
	GetConsoleScreenBufferInfo(console, &info);
	return info.srWindow.Right - info.srWindow.Left + 1;
#else
	if (ioctl(fileno(stdout), TIOCGWINSZ, &ws) < 0 || !ws.ws_col)
		return 0;
	return ws.ws_col;
#endif
}

