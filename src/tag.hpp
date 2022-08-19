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


void taglib_get_version(std::string &buffer);
bool tag_write_mp3(Track &track, Config &config);
bool tag_clear_mp3(Track &track, Config &config);

bool tag_write_flac(Track &track, Config &config);
bool tag_clear_flac(Track &track);

bool tag_write_ogg_vorbis(Track &track, Config &config);
bool tag_clear_ogg_vorbis(Track &track);

bool tag_write_ogg_flac(Track &track, Config &config);
bool tag_clear_ogg_flac(Track &track);

bool tag_write_ogg_speex(Track &track, Config &config);
bool tag_clear_ogg_speex(Track &track);

bool tag_write_ogg_opus(Track &track, Config &config);
bool tag_clear_ogg_opus(Track &track);

bool tag_write_mp4(Track &track, Config &config);
bool tag_clear_mp4(Track &track);

bool tag_write_asf(Track &track, Config &config);
bool tag_clear_asf(Track &track);

bool tag_write_wav(Track &track, Config &config);
bool tag_clear_wav(Track &track, Config &config);

bool tag_write_aiff(Track &track, Config &config);
bool tag_clear_aiff(Track &track, Config &config);

bool tag_write_wavpack(Track &track, Config &config);
bool tag_clear_wavpack(Track &track, Config &config);

bool tag_write_ape(Track &track, Config &config);
bool tag_clear_ape(Track &track, Config &config);

int gain_to_q78num(double gain);
