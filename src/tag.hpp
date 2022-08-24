/*
 * Loudness normalizer based on the EBU R128 standard
 *
 * Copyright (c) 2014, Alessandro Ghedini
 * All rights reserved.
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

#include "scan.hpp"

#define FORMAT_GAIN(gain) fmt::format("{:.2f} dB", gain)
#define FORMAT_PEAK(peak) fmt::format("{:.6f}", peak)
#define GAIN_TO_Q78(gain) (int) std::round(gain * 256.f)

#define RG_TAGS_UPPERCASE 0x1
#define RG_TAGS_LOWERCASE 0x10
#define R128_TAGS         0x100

#define MP4_ATOM_STRING "----:com.apple.iTunes:"
#define FORMAT_MP4_TAG(s, tag) s.append(MP4_ATOM_STRING).append(tag)
#define tag_error(t) output_error("Couldn't write to: {}", t.path)

void tag_track(Track &track, const Config &config);
void taglib_get_version(std::string &buffer);

static bool tag_mp3(Track &track, const Config &config);
static bool tag_flac(Track &track, const Config &config);
template<typename T>
static bool tag_ogg(Track &track, const Config &config);
static bool tag_mp4(Track &track, const Config &config);
template <typename T>
static bool tag_apev2(Track &track, const Config &config);
static bool tag_wma(Track &track, const Config &config);
template<typename T>
static bool tag_riff(Track &track, const Config &config);