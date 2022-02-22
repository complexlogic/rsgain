/*
 * Loudness normalizer based on the EBU R128 standard
 *
 * Copyright (c) 2014, Alessandro Ghedini
 * All rights reserved.
 * 2019-06-30 - Matthias C. Hormann
 *   - Tag format in accordance with ReplayGain 2.0 spec
 *     https://wiki.hydrogenaud.io/index.php?title=ReplayGain_2.0_specification
 *   - Add Ogg Vorbis file handling
 * 2019-07-07 - v0.2.3 - Matthias C. Hormann
 *   - Write lowercase REPLAYGAIN_* tags to MP3 ID3v2, for incompatible players
 * 2019-07-08 - v0.2.4 - Matthias C. Hormann
 *   - add -s e mode, writes extra tags (REPLAYGAIN_REFERENCE_LOUDNESS,
 *     REPLAYGAIN_TRACK_RANGE and REPLAYGAIN_ALBUM_RANGE)
 *   - add "-s l" mode (like "-s e" but uses LU/LUFS instead of dB)
 * 2019-07-09 - v0.2.6 - Matthias C. Hormann
 *  - Add "-L" mode to force lowercase tags in MP3/ID3v2.
 * 2019-07-10 - v0.2.7 - Matthias C. Hormann
 *  - Add "-S" mode to strip ID3v1/APEv2 tags from MP3 files.
 *  - Add "-I 3"/"-I 4" modes to select ID3v2 version to write.
 * 2019-07-31 - v0.40 - Matthias C. Hormann
 *  - Add MP4 handling
 * 2019-08-02 - v0.5.1 - Matthias C. Hormann
 *  - avoid unneccessary double file write on deleting+writing tags
 *  - make tag delete/write functions return true on success, false otherwise
 * 2019-08-06 - v0.5.3 - Matthias C. Hormann
 *  - Add support for Opus (.opus) files.
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

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <taglib.h>

#define TAGLIB_VERSION (TAGLIB_MAJOR_VERSION * 10000 \
                        + TAGLIB_MINOR_VERSION * 100 \
                        + TAGLIB_PATCH_VERSION)

#include <textidentificationframe.h>

#include <mpegfile.h>
#include <id3v2tag.h>
#include <apetag.h>

#include <flacfile.h>
#include <vorbisfile.h>
#include <oggflacfile.h>
#include <speexfile.h>
#include <xiphcomment.h>
#include <mp4file.h>
#include <opusfile.h>
#include <asffile.h>
// #include <rifffile.h>
#include <wavfile.h>
#include <aifffile.h>
#include <wavpackfile.h>
#include <apefile.h>

#include "scan.h"
#include "tag.h"
#include "output.h"

// define possible replaygain tags

enum RG_ENUM {
    RG_TRACK_GAIN,
    RG_TRACK_PEAK,
    RG_TRACK_RANGE,
    RG_ALBUM_GAIN,
    RG_ALBUM_PEAK,
    RG_ALBUM_RANGE,
    RG_REFERENCE_LOUDNESS
};

static const char *RG_STRING_UPPER[] = {
    "REPLAYGAIN_TRACK_GAIN",
    "REPLAYGAIN_TRACK_PEAK",
    "REPLAYGAIN_TRACK_RANGE",
    "REPLAYGAIN_ALBUM_GAIN",
    "REPLAYGAIN_ALBUM_PEAK",
    "REPLAYGAIN_ALBUM_RANGE",
    "REPLAYGAIN_REFERENCE_LOUDNESS"
};

static const char *RG_STRING_LOWER[] = {
    "replaygain_track_gain",
    "replaygain_track_peak",
    "replaygain_track_range",
    "replaygain_album_gain",
    "replaygain_album_peak",
    "replaygain_album_range",
    "replaygain_reference_loudness"
};

// this is where we store the RG tags in MP4/M4A files
static const char *RG_ATOM = "----:com.apple.iTunes:";


/*** MP3 ****/

static void tag_add_txxx(TagLib::ID3v2::Tag *tag, char *name, char *value) {
  TagLib::ID3v2::UserTextIdentificationFrame *frame =
    new TagLib::ID3v2::UserTextIdentificationFrame;

  frame -> setDescription(name);
  frame -> setText(value);

  tag -> addFrame(frame);
}

void tag_remove_mp3(TagLib::ID3v2::Tag *tag) {
  TagLib::ID3v2::FrameList::Iterator it;
  TagLib::ID3v2::FrameList frames = tag -> frameList("TXXX");

  for (it = frames.begin(); it != frames.end(); ++it) {
    TagLib::ID3v2::UserTextIdentificationFrame *frame =
     dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame*>(*it);

     // this removes all variants of upper-/lower-/mixed-case tags
    if (frame && frame -> fieldList().size() >= 2) {
      TagLib::String desc = frame -> description().upper();

      // also remove (old) reference loudness, it might be wrong after recalc
      if ((desc == RG_STRING_UPPER[RG_TRACK_GAIN]) ||
          (desc == RG_STRING_UPPER[RG_TRACK_PEAK]) ||
          (desc == RG_STRING_UPPER[RG_TRACK_RANGE]) ||
          (desc == RG_STRING_UPPER[RG_ALBUM_GAIN]) ||
          (desc == RG_STRING_UPPER[RG_ALBUM_PEAK]) ||
          (desc == RG_STRING_UPPER[RG_ALBUM_RANGE]) ||
          (desc == RG_STRING_UPPER[RG_REFERENCE_LOUDNESS]))
        tag -> removeFrame(frame);
    }
  }
}

// Even if the ReplayGain 2 standard proposes replaygain tags to be uppercase,
// unfortunately some players only respect the lowercase variant (still).
// So we use the "lowercase" flag to switch.
bool tag_write_mp3(scan_result *scan, bool do_album, char mode, char *unit,
  bool lowercase, bool strip, int id3v2version) {
  char value[2048];
  const char **RG_STRING = RG_STRING_UPPER;

  if (lowercase) {
    RG_STRING = RG_STRING_LOWER;
  }

  TagLib::MPEG::File f(scan -> file);
  TagLib::ID3v2::Tag *tag = f.ID3v2Tag(true);

  // remove old tags before writing new ones
  tag_remove_mp3(tag);

  snprintf(value, sizeof(value), "%.2f %s", scan -> track_gain, unit);
  tag_add_txxx(tag, const_cast<char *>(RG_STRING[RG_TRACK_GAIN]), value);

  snprintf(value, sizeof(value), "%.6f", scan -> track_peak);
  tag_add_txxx(tag, const_cast<char *>(RG_STRING[RG_TRACK_PEAK]), value);

  // Only write album tags if in album mode (would be zero otherwise)
  if (do_album) {
    snprintf(value, sizeof(value), "%.2f %s", scan -> album_gain, unit);
    tag_add_txxx(tag, const_cast<char *>(RG_STRING[RG_ALBUM_GAIN]), value);

    snprintf(value, sizeof(value), "%.6f", scan -> album_peak);
    tag_add_txxx(tag, const_cast<char *>(RG_STRING[RG_ALBUM_PEAK]), value);
  }

  // extra tags mode -s e or -s l
  if (mode == 'e' || mode == 'l') {
    snprintf(value, sizeof(value), "%.2f LUFS", scan -> loudness_reference);
    tag_add_txxx(tag, const_cast<char *>(RG_STRING[RG_REFERENCE_LOUDNESS]), value);

    snprintf(value, sizeof(value), "%.2f %s", scan -> track_loudness_range, unit);
    tag_add_txxx(tag, const_cast<char *>(RG_STRING[RG_TRACK_RANGE]), value);

    if (do_album) {
      snprintf(value, sizeof(value), "%.2f %s", scan -> album_loudness_range, unit);
      tag_add_txxx(tag, const_cast<char *>(RG_STRING[RG_ALBUM_RANGE]), value);
    }
  }

  // work around bug taglib/taglib#913: strip APE before ID3v1
  if (strip)
    f.strip(TagLib::MPEG::File::APE);

#if TAGLIB_VERSION >= 11200
  return f.save(TagLib::MPEG::File::ID3v2,
    strip ? TagLib::MPEG::File::StripOthers : TagLib::MPEG::File::StripNone,
    id3v2version == 3 ? TagLib::ID3v2::v3 : TagLib::ID3v2::v4);
#else
  return f.save(TagLib::MPEG::File::ID3v2, strip, id3v2version);
#endif
}

bool tag_clear_mp3(scan_result *scan, bool strip, int id3v2version) {
  TagLib::MPEG::File f(scan -> file);
  TagLib::ID3v2::Tag *tag = f.ID3v2Tag(true);

  tag_remove_mp3(tag);

  // work around bug taglib/taglib#913: strip APE before ID3v1
  if (strip)
    f.strip(TagLib::MPEG::File::APE);

#if TAGLIB_VERSION >= 11200
  return f.save(TagLib::MPEG::File::ID3v2,
    strip ? TagLib::MPEG::File::StripOthers : TagLib::MPEG::File::StripNone,
    id3v2version == 3 ? TagLib::ID3v2::v3 : TagLib::ID3v2::v4);
#else
  return f.save(TagLib::MPEG::File::ID3v2, strip, id3v2version);
#endif
}


/*** FLAC ****/

void tag_remove_flac(TagLib::Ogg::XiphComment *tag) {
  tag -> removeFields(RG_STRING_UPPER[RG_TRACK_GAIN]);
  tag -> removeFields(RG_STRING_UPPER[RG_TRACK_PEAK]);
  tag -> removeFields(RG_STRING_UPPER[RG_TRACK_RANGE]);
  tag -> removeFields(RG_STRING_UPPER[RG_ALBUM_GAIN]);
  tag -> removeFields(RG_STRING_UPPER[RG_ALBUM_PEAK]);
  tag -> removeFields(RG_STRING_UPPER[RG_ALBUM_RANGE]);
  tag -> removeFields(RG_STRING_UPPER[RG_REFERENCE_LOUDNESS]);
}

bool tag_write_flac(scan_result *scan, bool do_album, char mode, char *unit) {
  char value[2048];

  TagLib::FLAC::File f(scan -> file);
  TagLib::Ogg::XiphComment *tag = f.xiphComment(true);

  // remove old tags before writing new ones
  tag_remove_flac(tag);

  snprintf(value, sizeof(value), "%.2f %s", scan -> track_gain, unit);
  tag -> addField(RG_STRING_UPPER[RG_TRACK_GAIN], value);

  snprintf(value, sizeof(value), "%.6f", scan -> track_peak);
  tag -> addField(RG_STRING_UPPER[RG_TRACK_PEAK], value);

  // Only write album tags if in album mode (would be zero otherwise)
  if (do_album) {
    snprintf(value, sizeof(value), "%.2f %s", scan -> album_gain, unit);
    tag -> addField(RG_STRING_UPPER[RG_ALBUM_GAIN], value);

    snprintf(value, sizeof(value), "%.6f", scan -> album_peak);
    tag -> addField(RG_STRING_UPPER[RG_ALBUM_PEAK], value);
  }

  // extra tags mode -s e or -s l
  if (mode == 'e' || mode == 'l') {
    snprintf(value, sizeof(value), "%.2f LUFS", scan -> loudness_reference);
    tag -> addField(RG_STRING_UPPER[RG_REFERENCE_LOUDNESS], value);

    snprintf(value, sizeof(value), "%.2f %s", scan -> track_loudness_range, unit);
    tag -> addField(RG_STRING_UPPER[RG_TRACK_RANGE], value);

    if (do_album) {
      snprintf(value, sizeof(value), "%.2f %s", scan -> album_loudness_range, unit);
      tag -> addField(RG_STRING_UPPER[RG_ALBUM_RANGE], value);
    }
  }

  return f.save();
}

bool tag_clear_flac(scan_result *scan) {
  TagLib::FLAC::File f(scan -> file);
  TagLib::Ogg::XiphComment *tag = f.xiphComment(true);

  tag_remove_flac(tag);

  return f.save();
}


/*** Ogg (Vorbis, FLAC, Speex, Opus) ****/

void tag_remove_ogg(TagLib::Ogg::XiphComment *tag) {
  tag -> removeFields(RG_STRING_UPPER[RG_TRACK_GAIN]);
  tag -> removeFields(RG_STRING_UPPER[RG_TRACK_PEAK]);
  tag -> removeFields(RG_STRING_UPPER[RG_TRACK_RANGE]);
  tag -> removeFields(RG_STRING_UPPER[RG_ALBUM_GAIN]);
  tag -> removeFields(RG_STRING_UPPER[RG_ALBUM_PEAK]);
  tag -> removeFields(RG_STRING_UPPER[RG_ALBUM_RANGE]);
  tag -> removeFields(RG_STRING_UPPER[RG_REFERENCE_LOUDNESS]);
}

void tag_make_ogg(scan_result *scan, bool do_album, char mode, char *unit,
  TagLib::Ogg::XiphComment *tag) {

  char value[2048];

  // remove old tags before writing new ones
  tag_remove_ogg(tag);

  snprintf(value, sizeof(value), "%.2f %s", scan -> track_gain, unit);
  tag -> addField(RG_STRING_UPPER[RG_TRACK_GAIN], value);

  snprintf(value, sizeof(value), "%.6f", scan -> track_peak);
  tag -> addField(RG_STRING_UPPER[RG_TRACK_PEAK], value);

  // Only write album tags if in album mode (would be zero otherwise)
  if (do_album) {
    snprintf(value, sizeof(value), "%.2f %s", scan -> album_gain, unit);
    tag -> addField(RG_STRING_UPPER[RG_ALBUM_GAIN], value);

    snprintf(value, sizeof(value), "%.6f", scan -> album_peak);
    tag -> addField(RG_STRING_UPPER[RG_ALBUM_PEAK], value);
  }

  // extra tags mode -s e or -s l
  if (mode == 'e' || mode == 'l') {
    snprintf(value, sizeof(value), "%.2f LUFS", scan -> loudness_reference);
    tag -> addField(RG_STRING_UPPER[RG_REFERENCE_LOUDNESS], value);

    snprintf(value, sizeof(value), "%.2f %s", scan -> track_loudness_range, unit);
    tag -> addField(RG_STRING_UPPER[RG_TRACK_RANGE], value);

    if (do_album) {
      snprintf(value, sizeof(value), "%.2f %s", scan -> album_loudness_range, unit);
      tag -> addField(RG_STRING_UPPER[RG_ALBUM_RANGE], value);
    }
  }
}

/*** Ogg: Ogg Vorbis ***/

bool tag_write_ogg_vorbis(scan_result *scan, bool do_album, char mode, char *unit) {
  TagLib::Ogg::Vorbis::File f(scan -> file);
  TagLib::Ogg::XiphComment *tag = f.tag();

  tag_make_ogg(scan, do_album, mode, unit, tag);

  return f.save();
}

bool tag_clear_ogg_vorbis(scan_result *scan) {
  TagLib::Ogg::Vorbis::File f(scan -> file);
  TagLib::Ogg::XiphComment *tag = f.tag();

  tag_remove_ogg(tag);

  return f.save();
}

/*** Ogg: Ogg FLAC ***/

bool tag_write_ogg_flac(scan_result *scan, bool do_album, char mode, char *unit) {
  TagLib::Ogg::FLAC::File f(scan -> file);
  TagLib::Ogg::XiphComment *tag = f.tag();

  tag_make_ogg(scan, do_album, mode, unit, tag);

  return f.save();
}

bool tag_clear_ogg_flac(scan_result *scan) {
  TagLib::Ogg::FLAC::File f(scan -> file);
  TagLib::Ogg::XiphComment *tag = f.tag();

  tag_remove_ogg(tag);

  return f.save();
}

/*** Ogg: Ogg Speex ***/

bool tag_write_ogg_speex(scan_result *scan, bool do_album, char mode, char *unit) {
  TagLib::Ogg::Speex::File f(scan -> file);
  TagLib::Ogg::XiphComment *tag = f.tag();

  tag_make_ogg(scan, do_album, mode, unit, tag);

  return f.save();
}

bool tag_clear_ogg_speex(scan_result *scan) {
  TagLib::Ogg::Speex::File f(scan -> file);
  TagLib::Ogg::XiphComment *tag = f.tag();

  tag_remove_ogg(tag);

  return f.save();
}

/*** Ogg: Opus ****/

// Opus Notes:
//
// 1. Opus ONLY uses R128_TRACK_GAIN and (optionally) R128_ALBUM_GAIN
//    as an ADDITIONAL offset to the header's 'output_gain'.
// 2. Encoders and muxes set 'output_gain' to zero, so a non-zero 'output_gain' in
//    the header i supposed to be a change AFTER encoding/muxing.
// 3. We assume that FFmpeg's avformat does already apply 'output_gain' (???)
//    so we get get pre-gained data and only have to calculate the difference.
// 4. Opus adheres to EBU-R128, so the loudness reference is ALWAYS -23 LUFS.
//    This means we have to adapt for possible `-d n` (`--pregain=n`) changes.
//    This also means players have to add an extra +5 dB to reach the loudness
//    ReplayGain 2.0 prescribes (-18 LUFS).
// 5. Opus R128_* tags use ASCII-encoded Q7.8 numbers with max. 6 places including
//    the minus sign, and no unit.
//    See https://en.wikipedia.org/wiki/Q_(number_format)
// 6. RFC 7845 states: "To avoid confusion with multiple normalization schemes, an
//    Opus comment header SHOULD NOT contain any of the REPLAYGAIN_TRACK_GAIN,
//    REPLAYGAIN_TRACK_PEAK, REPLAYGAIN_ALBUM_GAIN, or REPLAYGAIN_ALBUM_PEAK tags, […]"
//    So we remove REPLAYGAIN_* tags if any are present.
// 7. RFC 7845 states: "Peak normalizations are difficult to calculate reliably
//    for lossy codecs because of variation in excursion heights due to decoder
//    differences. In the authors' investigations, they were not applied
//    consistently or broadly enough to merit inclusion here."
//    So there are NO "Peak" type tags. The (oversampled) true peak levels that
//    libebur128 calculates for us are STILL used for clipping prevention if so
//    requested. They are also shown in the output, just not stored into tags.

int gain_to_q78num(double gain) {
  // convert float to Q7.8 number: Q = round(f * 2^8)
  return (int) round(gain * 256.0);    // 2^8 = 256
}

void tag_remove_ogg_opus(TagLib::Ogg::XiphComment *tag) {
  // RFC 7845 states:
  // To avoid confusion with multiple normalization schemes, an Opus
  // comment header SHOULD NOT contain any of the REPLAYGAIN_TRACK_GAIN,
  // REPLAYGAIN_TRACK_PEAK, REPLAYGAIN_ALBUM_GAIN, or
  // REPLAYGAIN_ALBUM_PEAK tags, […]"
  // so we remove these if present
  tag -> removeFields(RG_STRING_UPPER[RG_TRACK_GAIN]);
  tag -> removeFields(RG_STRING_UPPER[RG_TRACK_PEAK]);
  tag -> removeFields(RG_STRING_UPPER[RG_TRACK_RANGE]);
  tag -> removeFields(RG_STRING_UPPER[RG_ALBUM_GAIN]);
  tag -> removeFields(RG_STRING_UPPER[RG_ALBUM_PEAK]);
  tag -> removeFields(RG_STRING_UPPER[RG_ALBUM_RANGE]);
  tag -> removeFields(RG_STRING_UPPER[RG_REFERENCE_LOUDNESS]);
  tag -> removeFields("R128_TRACK_GAIN");
  tag -> removeFields("R128_ALBUM_GAIN");
}

bool tag_write_ogg_opus(scan_result *scan, bool do_album, char mode, char *unit) {
  char value[2048];

  TagLib::Ogg::Opus::File f(scan -> file);
  TagLib::Ogg::XiphComment *tag = f.tag();

  // remove old tags before writing new ones
  tag_remove_ogg_opus(tag);

  snprintf(value, sizeof(value), "%d", gain_to_q78num(scan -> track_gain));
  tag -> addField("R128_TRACK_GAIN", value);

  // Only write album tags if in album mode (would be zero otherwise)
  if (do_album) {
    snprintf(value, sizeof(value), "%d", gain_to_q78num(scan -> album_gain));
    tag -> addField("R128_ALBUM_GAIN", value);
  }

  // extra tags mode -s e or -s l
  // no extra tags allowed in Opus

  return f.save();
}

bool tag_clear_ogg_opus(scan_result *scan) {
  TagLib::Ogg::Opus::File f(scan -> file);
  TagLib::Ogg::XiphComment *tag = f.tag();

  tag_remove_ogg_opus(tag);

  return f.save();
}


/*** MP4 ****/

// build tagging key from RG_ATOM and REPLAYGAIN_* string
TagLib::String tagname(TagLib::String key) {
  TagLib::String res = RG_ATOM;
  return res.append(key);
}

void tag_remove_mp4(TagLib::MP4::Tag *tag) {
  TagLib::String desc;
#if TAGLIB_VERSION >= 11200
  TagLib::MP4::ItemMap items = tag->itemMap();

  for(TagLib::MP4::ItemMap::Iterator item = items.begin();
      item != items.end(); ++item)
#else
  TagLib::MP4::ItemListMap &items = tag->itemListMap();

  for(TagLib::MP4::ItemListMap::Iterator item = items.begin();
      item != items.end(); ++item)
#endif
  {
    desc = item->first.upper();
    if ((desc == tagname(RG_STRING_UPPER[RG_TRACK_GAIN]).upper()) ||
        (desc == tagname(RG_STRING_UPPER[RG_TRACK_PEAK]).upper()) ||
        (desc == tagname(RG_STRING_UPPER[RG_TRACK_RANGE]).upper()) ||
        (desc == tagname(RG_STRING_UPPER[RG_ALBUM_GAIN]).upper()) ||
        (desc == tagname(RG_STRING_UPPER[RG_ALBUM_PEAK]).upper()) ||
        (desc == tagname(RG_STRING_UPPER[RG_ALBUM_RANGE]).upper()) ||
        (desc == tagname(RG_STRING_UPPER[RG_REFERENCE_LOUDNESS]).upper()))
      tag -> removeItem(item->first);
  }
}

bool tag_write_mp4(scan_result *scan, bool do_album, char mode, char *unit,
  bool lowercase) {
  char value[2048];
  const char **RG_STRING = RG_STRING_UPPER;

  if (lowercase) {
    RG_STRING = RG_STRING_LOWER;
  }

  TagLib::MP4::File f(scan -> file);
  TagLib::MP4::Tag *tag = f.tag();

  // remove old tags before writing new ones
  tag_remove_mp4(tag);

  snprintf(value, sizeof(value), "%.2f %s", scan -> track_gain, unit);
  tag -> setItem(tagname(RG_STRING[RG_TRACK_GAIN]), TagLib::StringList(value));

  snprintf(value, sizeof(value), "%.6f", scan -> track_peak);
  tag -> setItem(tagname(RG_STRING[RG_TRACK_PEAK]), TagLib::StringList(value));

  // Only write album tags if in album mode (would be zero otherwise)
  if (do_album) {
    snprintf(value, sizeof(value), "%.2f %s", scan -> album_gain, unit);
    tag -> setItem(tagname(RG_STRING[RG_ALBUM_GAIN]), TagLib::StringList(value));

    snprintf(value, sizeof(value), "%.6f", scan -> album_peak);
    tag -> setItem(tagname(RG_STRING[RG_ALBUM_PEAK]), TagLib::StringList(value));
  }

  // extra tags mode -s e or -s l
  if (mode == 'e' || mode == 'l') {
    snprintf(value, sizeof(value), "%.2f LUFS", scan -> loudness_reference);
    tag -> setItem(tagname(RG_STRING[RG_REFERENCE_LOUDNESS]), TagLib::StringList(value));

    snprintf(value, sizeof(value), "%.2f %s", scan -> track_loudness_range, unit);
    tag -> setItem(tagname(RG_STRING[RG_TRACK_RANGE]), TagLib::StringList(value));

    if (do_album) {
      snprintf(value, sizeof(value), "%.2f %s", scan -> album_loudness_range, unit);
      tag -> setItem(tagname(RG_STRING[RG_ALBUM_RANGE]), TagLib::StringList(value));
    }
  }

  return f.save();
}

bool tag_clear_mp4(scan_result *scan) {
  TagLib::MP4::File f(scan -> file);
  TagLib::MP4::Tag *tag = f.tag();

  tag_remove_mp4(tag);

  return f.save();
}


/*** ASF/WMA ****/

void tag_remove_asf(TagLib::ASF::Tag *tag) {
  TagLib::String desc;
  TagLib::ASF::AttributeListMap &items = tag->attributeListMap();

  for(TagLib::ASF::AttributeListMap::Iterator item = items.begin();
      item != items.end(); ++item)
  {
    desc = item->first.upper();
    if ((desc == RG_STRING_UPPER[RG_TRACK_GAIN]) ||
        (desc == RG_STRING_UPPER[RG_TRACK_PEAK]) ||
        (desc == RG_STRING_UPPER[RG_TRACK_RANGE]) ||
        (desc == RG_STRING_UPPER[RG_ALBUM_GAIN]) ||
        (desc == RG_STRING_UPPER[RG_ALBUM_PEAK]) ||
        (desc == RG_STRING_UPPER[RG_ALBUM_RANGE]) ||
        (desc == RG_STRING_UPPER[RG_REFERENCE_LOUDNESS]))
      tag -> removeItem(item->first);
  }
}

bool tag_write_asf(scan_result *scan, bool do_album, char mode, char *unit,
  bool lowercase) {
  char value[2048];
  const char **RG_STRING = RG_STRING_UPPER;

  if (lowercase) {
    RG_STRING = RG_STRING_LOWER;
  }

  TagLib::ASF::File f(scan -> file);
  TagLib::ASF::Tag *tag = f.tag();

  // remove old tags before writing new ones
  tag_remove_asf(tag);

  snprintf(value, sizeof(value), "%.2f %s", scan -> track_gain, unit);
  tag -> setAttribute(RG_STRING[RG_TRACK_GAIN], TagLib::String(value));

  snprintf(value, sizeof(value), "%.6f", scan -> track_peak);
  tag -> setAttribute(RG_STRING[RG_TRACK_PEAK], TagLib::String(value));

  // Only write album tags if in album mode (would be zero otherwise)
  if (do_album) {
    snprintf(value, sizeof(value), "%.2f %s", scan -> album_gain, unit);
    tag -> setAttribute(RG_STRING[RG_ALBUM_GAIN], TagLib::String(value));

    snprintf(value, sizeof(value), "%.6f", scan -> album_peak);
    tag -> setAttribute(RG_STRING[RG_ALBUM_PEAK], TagLib::String(value));
  }

  // extra tags mode -s e or -s l
  if (mode == 'e' || mode == 'l') {
    snprintf(value, sizeof(value), "%.2f LUFS", scan -> loudness_reference);
    tag -> setAttribute(RG_STRING[RG_REFERENCE_LOUDNESS], TagLib::String(value));

    snprintf(value, sizeof(value), "%.2f %s", scan -> track_loudness_range, unit);
    tag -> setAttribute(RG_STRING[RG_TRACK_RANGE], TagLib::String(value));

    if (do_album) {
      snprintf(value, sizeof(value), "%.2f %s", scan -> album_loudness_range, unit);
      tag -> setAttribute(RG_STRING[RG_ALBUM_RANGE], TagLib::String(value));
    }
  }

  return f.save();
}

bool tag_clear_asf(scan_result *scan) {
  TagLib::ASF::File f(scan -> file);
  TagLib::ASF::Tag *tag = f.tag();

  tag_remove_asf(tag);

  return f.save();
}


/*** WAV ****/

void tag_remove_wav(TagLib::ID3v2::Tag *tag) {
  TagLib::ID3v2::FrameList::Iterator it;
  TagLib::ID3v2::FrameList frames = tag -> frameList("TXXX");

  for (it = frames.begin(); it != frames.end(); ++it) {
    TagLib::ID3v2::UserTextIdentificationFrame *frame =
     dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame*>(*it);

     // this removes all variants of upper-/lower-/mixed-case tags
    if (frame && frame -> fieldList().size() >= 2) {
      TagLib::String desc = frame -> description().upper();

      // also remove (old) reference loudness, it might be wrong after recalc
      if ((desc == RG_STRING_UPPER[RG_TRACK_GAIN]) ||
          (desc == RG_STRING_UPPER[RG_TRACK_PEAK]) ||
          (desc == RG_STRING_UPPER[RG_TRACK_RANGE]) ||
          (desc == RG_STRING_UPPER[RG_ALBUM_GAIN]) ||
          (desc == RG_STRING_UPPER[RG_ALBUM_PEAK]) ||
          (desc == RG_STRING_UPPER[RG_ALBUM_RANGE]) ||
          (desc == RG_STRING_UPPER[RG_REFERENCE_LOUDNESS]))
        tag -> removeFrame(frame);
    }
  }
}

// Experimental WAV file tagging within an "ID3 " chunk
bool tag_write_wav(scan_result *scan, bool do_album, char mode, char *unit,
  bool lowercase, bool strip, int id3v2version) {
  char value[2048];
  const char **RG_STRING = RG_STRING_UPPER;

  if (lowercase) {
    RG_STRING = RG_STRING_LOWER;
  }

  TagLib::RIFF::WAV::File f(scan -> file);
  TagLib::ID3v2::Tag *tag = f.ID3v2Tag();

  // remove old tags before writing new ones
  tag_remove_wav(tag);

  snprintf(value, sizeof(value), "%.2f %s", scan -> track_gain, unit);
  tag_add_txxx(tag, const_cast<char *>(RG_STRING[RG_TRACK_GAIN]), value);

  snprintf(value, sizeof(value), "%.6f", scan -> track_peak);
  tag_add_txxx(tag, const_cast<char *>(RG_STRING[RG_TRACK_PEAK]), value);

  // Only write album tags if in album mode (would be zero otherwise)
  if (do_album) {
    snprintf(value, sizeof(value), "%.2f %s", scan -> album_gain, unit);
    tag_add_txxx(tag, const_cast<char *>(RG_STRING[RG_ALBUM_GAIN]), value);

    snprintf(value, sizeof(value), "%.6f", scan -> album_peak);
    tag_add_txxx(tag, const_cast<char *>(RG_STRING[RG_ALBUM_PEAK]), value);
  }

  // extra tags mode -s e or -s l
  if (mode == 'e' || mode == 'l') {
    snprintf(value, sizeof(value), "%.2f LUFS", scan -> loudness_reference);
    tag_add_txxx(tag, const_cast<char *>(RG_STRING[RG_REFERENCE_LOUDNESS]), value);

    snprintf(value, sizeof(value), "%.2f %s", scan -> track_loudness_range, unit);
    tag_add_txxx(tag, const_cast<char *>(RG_STRING[RG_TRACK_RANGE]), value);

    if (do_album) {
      snprintf(value, sizeof(value), "%.2f %s", scan -> album_loudness_range, unit);
      tag_add_txxx(tag, const_cast<char *>(RG_STRING[RG_ALBUM_RANGE]), value);
    }
  }

  // no stripping
#if TAGLIB_VERSION >= 11200
  return f.save(TagLib::RIFF::WAV::File::AllTags,
    TagLib::RIFF::WAV::File::StripNone,
    id3v2version == 3 ? TagLib::ID3v2::v3 : TagLib::ID3v2::v4);
#else
  return f.save(TagLib::RIFF::WAV::File::AllTags, false, id3v2version);
#endif
}

bool tag_clear_wav(scan_result *scan, bool strip, int id3v2version) {
  TagLib::RIFF::WAV::File f(scan -> file);
  TagLib::ID3v2::Tag *tag = f.ID3v2Tag();

  tag_remove_wav(tag);

  // no stripping
#if TAGLIB_VERSION >= 11200
  return f.save(TagLib::RIFF::WAV::File::AllTags,
    TagLib::RIFF::WAV::File::StripNone,
    id3v2version == 3 ? TagLib::ID3v2::v3 : TagLib::ID3v2::v4);
#else
  return f.save(TagLib::RIFF::WAV::File::AllTags, false, id3v2version);
#endif
}


/*** AIFF ****/

// id3v2version and strip currently unimplemented since no TagLib support

void tag_remove_aiff(TagLib::ID3v2::Tag *tag) {
  TagLib::ID3v2::FrameList::Iterator it;
  TagLib::ID3v2::FrameList frames = tag -> frameList("TXXX");

  for (it = frames.begin(); it != frames.end(); ++it) {
    TagLib::ID3v2::UserTextIdentificationFrame *frame =
     dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame*>(*it);

     // this removes all variants of upper-/lower-/mixed-case tags
    if (frame && frame -> fieldList().size() >= 2) {
      TagLib::String desc = frame -> description().upper();

      // also remove (old) reference loudness, it might be wrong after recalc
      if ((desc == RG_STRING_UPPER[RG_TRACK_GAIN]) ||
          (desc == RG_STRING_UPPER[RG_TRACK_PEAK]) ||
          (desc == RG_STRING_UPPER[RG_TRACK_RANGE]) ||
          (desc == RG_STRING_UPPER[RG_ALBUM_GAIN]) ||
          (desc == RG_STRING_UPPER[RG_ALBUM_PEAK]) ||
          (desc == RG_STRING_UPPER[RG_ALBUM_RANGE]) ||
          (desc == RG_STRING_UPPER[RG_REFERENCE_LOUDNESS]))
        tag -> removeFrame(frame);
    }
  }
}

// Experimental AIFF file tagging within an "ID3 " chunk
bool tag_write_aiff(scan_result *scan, bool do_album, char mode, char *unit,
  bool lowercase, bool strip, int id3v2version) {
  char value[2048];
  const char **RG_STRING = RG_STRING_UPPER;

  if (lowercase) {
    RG_STRING = RG_STRING_LOWER;
  }

  TagLib::RIFF::AIFF::File f(scan -> file);
  TagLib::ID3v2::Tag *tag = f.tag();

  // remove old tags before writing new ones
  tag_remove_aiff(tag);

  snprintf(value, sizeof(value), "%.2f %s", scan -> track_gain, unit);
  tag_add_txxx(tag, const_cast<char *>(RG_STRING[RG_TRACK_GAIN]), value);

  snprintf(value, sizeof(value), "%.6f", scan -> track_peak);
  tag_add_txxx(tag, const_cast<char *>(RG_STRING[RG_TRACK_PEAK]), value);

  // Only write album tags if in album mode (would be zero otherwise)
  if (do_album) {
    snprintf(value, sizeof(value), "%.2f %s", scan -> album_gain, unit);
    tag_add_txxx(tag, const_cast<char *>(RG_STRING[RG_ALBUM_GAIN]), value);

    snprintf(value, sizeof(value), "%.6f", scan -> album_peak);
    tag_add_txxx(tag, const_cast<char *>(RG_STRING[RG_ALBUM_PEAK]), value);
  }

  // extra tags mode -s e or -s l
  if (mode == 'e' || mode == 'l') {
    snprintf(value, sizeof(value), "%.2f LUFS", scan -> loudness_reference);
    tag_add_txxx(tag, const_cast<char *>(RG_STRING[RG_REFERENCE_LOUDNESS]), value);

    snprintf(value, sizeof(value), "%.2f %s", scan -> track_loudness_range, unit);
    tag_add_txxx(tag, const_cast<char *>(RG_STRING[RG_TRACK_RANGE]), value);

    if (do_album) {
      snprintf(value, sizeof(value), "%.2f %s", scan -> album_loudness_range, unit);
      tag_add_txxx(tag, const_cast<char *>(RG_STRING[RG_ALBUM_RANGE]), value);
    }
  }

  // no stripping
#if TAGLIB_VERSION >= 11200
  return f.save(id3v2version == 3 ? TagLib::ID3v2::v3 : TagLib::ID3v2::v4);
#else
  return f.save();
#endif
}

bool tag_clear_aiff(scan_result *scan, bool strip, int id3v2version) {
  TagLib::RIFF::AIFF::File f(scan -> file);
  TagLib::ID3v2::Tag *tag = f.tag();

  tag_remove_aiff(tag);

  // no stripping
#if TAGLIB_VERSION >= 11200
  return f.save(id3v2version == 3 ? TagLib::ID3v2::v3 : TagLib::ID3v2::v4);
#else
  return f.save();
#endif
}


/*** WavPack ***/

// We COULD also use ID3 tags, but we stick with APEv2 tags,
// since that is the native format.
// APEv2 tags can be mixed case, but they should be read case-insensitively,
// so we currently ignore -L (--lowercase) and only write uppercase tags.
// TagLib handles APE case-insensitively and uses only UPPERCASE keys.
// Existing ID3v2 tags can be removed by using -S (--striptags).

void tag_remove_wavpack(TagLib::APE::Tag *tag) {
  tag -> removeItem(RG_STRING_UPPER[RG_TRACK_GAIN]);
  tag -> removeItem(RG_STRING_UPPER[RG_TRACK_PEAK]);
  tag -> removeItem(RG_STRING_UPPER[RG_TRACK_RANGE]);
  tag -> removeItem(RG_STRING_UPPER[RG_ALBUM_GAIN]);
  tag -> removeItem(RG_STRING_UPPER[RG_ALBUM_PEAK]);
  tag -> removeItem(RG_STRING_UPPER[RG_ALBUM_RANGE]);
  tag -> removeItem(RG_STRING_UPPER[RG_REFERENCE_LOUDNESS]);
}

bool tag_write_wavpack(scan_result *scan, bool do_album, char mode, char *unit,
  bool lowercase, bool strip) {
  char value[2048];
  const char **RG_STRING = RG_STRING_UPPER;

  // ignore lowercase for now: CAN be written but keys should be read case-insensitively
  // if (lowercase) {
  //   RG_STRING = RG_STRING_LOWER;
  // }

  TagLib::WavPack::File f(scan -> file);
  TagLib::APE::Tag *tag = f.APETag(true); // create if none exists

  // remove old tags before writing new ones
  tag_remove_wavpack(tag);

  snprintf(value, sizeof(value), "%.2f %s", scan -> track_gain, unit);
  tag -> addValue(RG_STRING[RG_TRACK_GAIN], TagLib::String(value), true);

  snprintf(value, sizeof(value), "%.6f", scan -> track_peak);
  tag -> addValue(RG_STRING[RG_TRACK_PEAK], TagLib::String(value), true);

  // Only write album tags if in album mode (would be zero otherwise)
  if (do_album) {
    snprintf(value, sizeof(value), "%.2f %s", scan -> album_gain, unit);
    tag -> addValue(RG_STRING[RG_ALBUM_GAIN], TagLib::String(value), true);

    snprintf(value, sizeof(value), "%.6f", scan -> album_peak);
    tag -> addValue(RG_STRING[RG_ALBUM_PEAK], TagLib::String(value), true);
  }

  // extra tags mode -s e or -s l
  if (mode == 'e' || mode == 'l') {
    snprintf(value, sizeof(value), "%.2f LUFS", scan -> loudness_reference);
    tag -> addValue(RG_STRING[RG_REFERENCE_LOUDNESS], TagLib::String(value), true);

    snprintf(value, sizeof(value), "%.2f %s", scan -> track_loudness_range, unit);
    tag -> addValue(RG_STRING[RG_TRACK_RANGE], TagLib::String(value), true);

    if (do_album) {
      snprintf(value, sizeof(value), "%.2f %s", scan -> album_loudness_range, unit);
      tag -> addValue(RG_STRING[RG_ALBUM_RANGE], TagLib::String(value), true);
    }
  }

  if (strip)
    f.strip(TagLib::WavPack::File::TagTypes::ID3v1);

  return f.save();
}

bool tag_clear_wavpack(scan_result *scan, bool strip) {

  TagLib::WavPack::File f(scan -> file);
  TagLib::APE::Tag *tag = f.APETag(true); // create if none exists

  tag_remove_wavpack(tag);

  if (strip)
    f.strip(TagLib::WavPack::File::TagTypes::ID3v1);

  return f.save();
}


/*** APE (Monkey’s Audio) ***/

// We COULD also use ID3 tags, but we stick with APEv2 tags,
// since that is the native format.
// APEv2 tags can be mixed case, but they should be read case-insensitively,
// so we currently ignore -L (--lowercase) and only write uppercase tags.
// TagLib handles APE case-insensitively and uses only UPPERCASE keys.
// Existing ID3 tags can be removed by using -S (--striptags).

void tag_remove_ape(TagLib::APE::Tag *tag) {
  tag -> removeItem(RG_STRING_UPPER[RG_TRACK_GAIN]);
  tag -> removeItem(RG_STRING_UPPER[RG_TRACK_PEAK]);
  tag -> removeItem(RG_STRING_UPPER[RG_TRACK_RANGE]);
  tag -> removeItem(RG_STRING_UPPER[RG_ALBUM_GAIN]);
  tag -> removeItem(RG_STRING_UPPER[RG_ALBUM_PEAK]);
  tag -> removeItem(RG_STRING_UPPER[RG_ALBUM_RANGE]);
  tag -> removeItem(RG_STRING_UPPER[RG_REFERENCE_LOUDNESS]);
}

bool tag_write_ape(scan_result *scan, bool do_album, char mode, char *unit,
  bool lowercase, bool strip) {
  char value[2048];
  const char **RG_STRING = RG_STRING_UPPER;

  // ignore lowercase for now: CAN be written but keys should be read case-insensitively
  // if (lowercase) {
  //   RG_STRING = RG_STRING_LOWER;
  // }

  TagLib::APE::File f(scan -> file);
  TagLib::APE::Tag *tag = f.APETag(true); // create if none exists

  // remove old tags before writing new ones
  tag_remove_ape(tag);

  snprintf(value, sizeof(value), "%.2f %s", scan -> track_gain, unit);
  tag -> addValue(RG_STRING[RG_TRACK_GAIN], TagLib::String(value), true);

  snprintf(value, sizeof(value), "%.6f", scan -> track_peak);
  tag -> addValue(RG_STRING[RG_TRACK_PEAK], TagLib::String(value), true);

  // Only write album tags if in album mode (would be zero otherwise)
  if (do_album) {
    snprintf(value, sizeof(value), "%.2f %s", scan -> album_gain, unit);
    tag -> addValue(RG_STRING[RG_ALBUM_GAIN], TagLib::String(value), true);

    snprintf(value, sizeof(value), "%.6f", scan -> album_peak);
    tag -> addValue(RG_STRING[RG_ALBUM_PEAK], TagLib::String(value), true);
  }

  // extra tags mode -s e or -s l
  if (mode == 'e' || mode == 'l') {
    snprintf(value, sizeof(value), "%.2f LUFS", scan -> loudness_reference);
    tag -> addValue(RG_STRING[RG_REFERENCE_LOUDNESS], TagLib::String(value), true);

    snprintf(value, sizeof(value), "%.2f %s", scan -> track_loudness_range, unit);
    tag -> addValue(RG_STRING[RG_TRACK_RANGE], TagLib::String(value), true);

    if (do_album) {
      snprintf(value, sizeof(value), "%.2f %s", scan -> album_loudness_range, unit);
      tag -> addValue(RG_STRING[RG_ALBUM_RANGE], TagLib::String(value), true);
    }
  }

  if (strip)
    f.strip(TagLib::APE::File::TagTypes::ID3v1);

  return f.save();
}

bool tag_clear_ape(scan_result *scan, bool strip) {

  TagLib::WavPack::File f(scan -> file);
  TagLib::APE::Tag *tag = f.APETag(true); // create if none exists

  tag_remove_ape(tag);

  if (strip)
    f.strip(TagLib::WavPack::File::TagTypes::ID3v1);

  return f.save();
}
