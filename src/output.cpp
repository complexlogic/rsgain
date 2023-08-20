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
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <syslog.h>
#include <limits.h>
#include <sys/ioctl.h>
#endif

#include <fmt/core.h>
#include "output.hpp"
#include "config.h"

#define MT_MESSAGE " Scanning directory: "

constexpr int str_literal_len(const char *str)
{
	return *str ? 1 + str_literal_len(str + 1) : 0;
}

void ProgressBar::begin(int start, int len)
{
	this->start = start;
	this->len = len;
}

void ProgressBar::update(int pos)
{
	int w, c;
	if (pos == pos_prev || !len)
		return;

	w = get_console_width();
#ifdef MAXPROGBARWIDTH
	if (w > MAXPROGBARWIDTH)
		w = MAXPROGBARWIDTH;
#endif
	w -= 8;
	if (w <= 0)
		return;

	if (w != w_prev) {
		delete buffer;
		buffer = new char[(size_t) (w + 3)];
	}

	float percent = (float) pos / (float) len;
	c = (int) (percent * (float) w);
	if (c > w)
		c = w;

	// Only output if we've actually made progress since last the call, or the console width changed
	if (c != c_prev || w != w_prev) {
		fmt::print(" {:3.0f}% [", percent * 100.f);
		memset(buffer, '=', (size_t) c);
		memset(buffer + c, ' ', (size_t) (w - c));
		buffer[w] = ']';
		buffer[w + 1] = '\r';
		buffer[w + 2] = '\0';
#ifdef _WIN32
		WriteConsoleA(console, buffer, w + 2, nullptr, nullptr);
#else
		fputs(buffer, stdout);
		fflush(stdout);
#endif
	}

	c_prev = c;
	w_prev = w;
	pos_prev = pos;
}

void ProgressBar::complete()
{
	if (c_prev != w_prev)
		update(len);

	delete buffer;
	buffer = nullptr;
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

void MTProgress::update(const std::string &path)
{
	if (quiet)
		return;
	static constexpr int w_message = 7 + str_literal_len(MT_MESSAGE);
	int w_console = ProgressBar::get_console_width();
	if (!w_console)
		return;
	int w_path = utf8_length(path);
	if (w_path + w_message >= w_console)
		w_path = w_console - w_message;

	fmt::print("\33[2K " COLOR_GREEN "{:5.1f}%" COLOR_OFF  MT_MESSAGE "{:.{}}\r", 
		100.f * ((float) (cur) / (float) (total)), 
		path,
		w_path < 0 ? 0 : w_path
	);
	fflush(stdout);
	cur++;
}

int MTProgress::utf8_length(std::string_view string)
{
    int length = 0;
    auto it = string.cbegin();
    while (it < string.cend()) {
        if ((*it & 0x80) == 0) // If byte is 0xxxxxxx, then it's a 1 byte (ASCII) char
            it++;
        else if ((*it & 0xE0) == 0xC0) // If byte is 110xxxxx, then it's a 2 byte char
            it +=2;
        else if ((*it & 0xF0) == 0xE0) // If byte is 1110xxxx, then it's a 3 byte char
            it +=3;
        else if ((*it & 0xF8) == 0xF0 || 1) // If byte is 11110xxx, then it's a 4 byte char
            it +=4;
        length++;
    }
    return length;
}
