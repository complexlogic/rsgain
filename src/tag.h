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

#ifdef __cplusplus
extern "C" {
#endif

bool tag_write_mp3(scan_result *scan, bool do_album, char mode, char *unit,
  bool lowercase, bool strip, int id3v2version);
bool tag_clear_mp3(scan_result *scan, bool strip, int id3v2version);

bool tag_write_flac(scan_result *scan, bool do_album, char mode, char *unit);
bool tag_clear_flac(scan_result *scan);

bool tag_write_ogg_vorbis(scan_result *scan, bool do_album, char mode, char *unit);
bool tag_clear_ogg_vorbis(scan_result *scan);

bool tag_write_ogg_flac(scan_result *scan, bool do_album, char mode, char *unit);
bool tag_clear_ogg_flac(scan_result *scan);

bool tag_write_ogg_speex(scan_result *scan, bool do_album, char mode, char *unit);
bool tag_clear_ogg_speex(scan_result *scan);

bool tag_write_ogg_opus(scan_result *scan, bool do_album, char mode, char *unit);
bool tag_clear_ogg_opus(scan_result *scan);

bool tag_write_mp4(scan_result *scan, bool do_album, char mode, char *unit,
  bool lowercase);
bool tag_clear_mp4(scan_result *scan);

bool tag_write_asf(scan_result *scan, bool do_album, char mode, char *unit,
  bool lowercase);
bool tag_clear_asf(scan_result *scan);

bool tag_write_wav(scan_result *scan, bool do_album, char mode, char *unit,
  bool lowercase, bool strip, int id3v2version);
bool tag_clear_wav(scan_result *scan, bool strip, int id3v2version);

bool tag_write_aiff(scan_result *scan, bool do_album, char mode, char *unit,
  bool lowercase, bool strip, int id3v2version);
bool tag_clear_aiff(scan_result *scan, bool strip, int id3v2version);

bool tag_write_wavpack(scan_result *scan, bool do_album, char mode, char *unit,
  bool lowercase, bool strip);
bool tag_clear_wavpack(scan_result *scan, bool strip);

bool tag_write_ape(scan_result *scan, bool do_album, char mode, char *unit,
  bool lowercase, bool strip);
bool tag_clear_ape(scan_result *scan, bool strip);

int gain_to_q78num(double gain);

#ifdef __cplusplus
}
#endif
