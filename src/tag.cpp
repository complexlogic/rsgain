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
 *         * Redistributions of source code must retain the above copyright
 *             notice, this list of conditions and the following disclaimer.
 *
 *         * Redistributions in binary form must reproduce the above copyright
 *             notice, this list of conditions and the following disclaimer in the
 *             documentation and/or other materials provided with the distribution.
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

#include <cmath>
#include <cstdio>
#include <string>
#include <array>
#include <memory>
#include <bit>

#include <taglib/taglib.h>
#include <taglib/fileref.h>
#include <taglib/textidentificationframe.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/apetag.h>
#include <taglib/flacfile.h>
#include <taglib/vorbisfile.h>
#include <taglib/oggflacfile.h>
#include <taglib/speexfile.h>
#include <taglib/xiphcomment.h>
#include <taglib/mp4file.h>
#include <taglib/opusfile.h>
#include <taglib/asffile.h>
#include <taglib/wavfile.h>
#include <taglib/aifffile.h>
#include <taglib/wavpackfile.h>
#include <taglib/apefile.h>
#include <taglib/mpcfile.h>
#include <libavcodec/avcodec.h>

#define CRCPP_USE_CPP11
#include "external/CRC.h"

#include "rsgain.hpp"
#include "scan.hpp"
#include "tag.hpp"
#include "output.hpp"

#define TAGLIB_VERSION (TAGLIB_MAJOR_VERSION * 10000 + TAGLIB_MINOR_VERSION * 100 + TAGLIB_PATCH_VERSION)
#define FORMAT_GAIN(gain) rsgain::format("{:.2f} dB", gain)
#define FORMAT_PEAK(peak) rsgain::format("{:.6f}", peak)
#define OPUS_HEADER_SIZE 47
#define OGG_ROW_SIZE 4
#define OPUS_HEAD_OFFSET 7 * OGG_ROW_SIZE
#define OGG_CRC_OFFSET 5 * OGG_ROW_SIZE + 2
#define OGG_SEGMENT_TABLE_OFFSET 27
#define OPUS_GAIN_OFFSET 11 * OGG_ROW_SIZE
#define RG_TAGS_UPPERCASE 1
#define RG_TAGS_LOWERCASE 2
#define R128_TAGS         4

#define MP4_ATOM_STRING "----:com.apple.iTunes:"
#define FORMAT_MP4_TAG(s, tag) s.append(MP4_ATOM_STRING).append(tag)

using RGTagsArray = std::array<TagLib::String, 7>;

static bool set_mpc_packet_rg(const char *path);
static bool tag_mp3(ScanJob::Track &track, const Config &config);
static bool tag_flac(ScanJob::Track &track, const Config &config);
template<typename T>
static bool tag_ogg(ScanJob::Track &track, const Config &config);
static bool tag_mp4(ScanJob::Track &track, const Config &config);
template <typename T>
static bool tag_apev2(ScanJob::Track &track, const Config &config);
static bool tag_wma(ScanJob::Track &track, const Config &config);
template<typename T>
static bool tag_riff(ScanJob::Track &track, const Config &config);
template<typename T>
static void write_rg_tags(const ScanResult &result, const Config &config, T&& write_tag);
template<int flags, typename T>
static void tag_clear_map(T&& clear);
static void tag_clear(TagLib::ID3v2::Tag *tag);
static void tag_write(TagLib::ID3v2::Tag *tag, const ScanResult &result, const Config &config);
template<typename T>
static void tag_clear(TagLib::Ogg::XiphComment *tag);
template<typename T>
static void tag_write(TagLib::Ogg::XiphComment *tag, const ScanResult &result, const Config &config);
static void tag_clear(TagLib::MP4::Tag *tag);
static void tag_write(TagLib::MP4::Tag *tag, const ScanResult &result, const Config &config);
static void tag_clear(TagLib::APE::Tag *tag);
static void tag_write(TagLib::APE::Tag *tag, const ScanResult &result, const Config &config);
static void tag_clear(TagLib::ASF::Tag *tag);
static void tag_write(TagLib::ASF::Tag *tag, const ScanResult &result, const Config &config);

template<typename T>
static bool tag_exists_id3(const ScanJob::Track &track);
template<typename T>
static bool tag_exists_xiph(const ScanJob::Track &track);
static bool tag_exists_mp4(const ScanJob::Track &track);
template<typename T>
static bool tag_exists_ape(const ScanJob::Track &track);
static bool tag_exists_asf(const ScanJob::Track &track);

enum class RGTag {
    TRACK_GAIN,
    TRACK_PEAK,
    TRACK_RANGE,
    ALBUM_GAIN,
    ALBUM_PEAK,
    ALBUM_RANGE,
    REFERENCE_LOUDNESS,
    MAX_VAL
};

static const RGTagsArray RG_STRING_UPPER = {{
    "REPLAYGAIN_TRACK_GAIN",
    "REPLAYGAIN_TRACK_PEAK",
    "REPLAYGAIN_TRACK_RANGE",
    "REPLAYGAIN_ALBUM_GAIN",
    "REPLAYGAIN_ALBUM_PEAK",
    "REPLAYGAIN_ALBUM_RANGE",
    "REPLAYGAIN_REFERENCE_LOUDNESS"
}};

static const RGTagsArray RG_STRING_LOWER = {{
    "replaygain_track_gain",
    "replaygain_track_peak",
    "replaygain_track_range",
    "replaygain_album_gain",
    "replaygain_album_peak",
    "replaygain_album_range",
    "replaygain_reference_loudness"
}};

static_assert((size_t) RGTag::MAX_VAL == RG_STRING_UPPER.size());
static_assert(RG_STRING_UPPER.size() == RG_STRING_LOWER.size());

enum class R128Tag {
    TRACK_GAIN,
    ALBUM_GAIN,
    MAX_VAL
};

static const std::array<TagLib::String, 2> R128_STRING = {{
    "R128_TRACK_GAIN",
    "R128_ALBUM_GAIN"
}};
static_assert((size_t) R128Tag::MAX_VAL == R128_STRING.size());

bool tag_track(ScanJob::Track &track, const Config &config)
{
    bool ret = false;
    switch (track.type) {
        case FileType::MP2:
        case FileType::MP3:
            ret = tag_mp3(track, config);
            break;

        case FileType::FLAC:
            ret = tag_flac(track, config);
            break;

        case FileType::OGG:
            switch (track.codec_id) {
                case AV_CODEC_ID_OPUS:
                    ret = tag_ogg<TagLib::Ogg::Opus::File>(track, config);
                    break;

                case AV_CODEC_ID_VORBIS:
                    ret = tag_ogg<TagLib::Ogg::Vorbis::File>(track, config);
                    break;

                case AV_CODEC_ID_FLAC:
                    ret = tag_ogg<TagLib::Ogg::FLAC::File>(track, config);
                    break;

                case AV_CODEC_ID_SPEEX:
                    ret = tag_ogg<TagLib::Ogg::Speex::File>(track, config);
                    break;

                default:
                    ret = tag_ogg<TagLib::FileRef>(track, config);
                    break;
            }
            break;
                
        case FileType::OPUS:
            ret = tag_ogg<TagLib::Ogg::Opus::File>(track, config);
            break;

        case FileType::M4A:
            ret = tag_mp4(track, config);
            break;

        case FileType::WMA:
            ret = tag_wma(track, config);
            break;

        case FileType::WAV:
            ret = tag_riff<TagLib::RIFF::WAV::File>(track, config);
            break;

        case FileType::AIFF:
            ret = tag_riff<TagLib::RIFF::AIFF::File>(track, config);
            break;

        case FileType::WAVPACK:
            ret = tag_apev2<TagLib::WavPack::File>(track, config);
            break;
            
        case FileType::APE:
        case FileType::TAK:
            ret = tag_apev2<TagLib::APE::File>(track, config);
            break;

        case FileType::MPC:
            ret = tag_apev2<TagLib::MPC::File>(track, config);
            break;

        default:
            break;
    }
    if (track.mtime)
        std::filesystem::last_write_time(track.path, *(track.mtime));
    if (!ret)
        output_error("Couldn't write tags to: {}", track.path.string());
    return ret;
}

bool tag_exists(const ScanJob::Track &track)
{
    switch(track.type) {
        case FileType::MP2:
        case FileType::MP3:
            return tag_exists_id3<TagLib::MPEG::File>(track);

        case FileType::FLAC:
            return tag_exists_xiph<TagLib::FLAC::File>(track);

        case FileType::OGG:
            return tag_exists_xiph<TagLib::FileRef>(track);

        case FileType::OPUS:
            return tag_exists_xiph<TagLib::Ogg::Opus::File>(track);

        case FileType::M4A:
            return tag_exists_mp4(track);

        case FileType::WMA:
            return tag_exists_asf(track);

        case FileType::WAV:
            return tag_exists_id3<TagLib::RIFF::WAV::File>(track);

        case FileType::AIFF:
            return tag_exists_id3<TagLib::RIFF::AIFF::File>(track);

        case FileType::WAVPACK:
            return tag_exists_ape<TagLib::WavPack::File>(track);
            
        case FileType::APE:
        case FileType::TAK:
            return tag_exists_ape<TagLib::APE::File>(track);

        case FileType::MPC:
            return tag_exists_ape<TagLib::MPC::File>(track);

        default:
            return false;
    }
    return false;
}

template<typename T>
static bool tag_exists_id3(const ScanJob::Track &track)
{
    const TagLib::ID3v2::Tag *tag = nullptr;
    T file(track.path.string().c_str(), false);
    if constexpr (std::is_same_v<T, TagLib::RIFF::AIFF::File>)
        tag = file.tag();
    else
        tag = file.ID3v2Tag();
    if (tag) {
        const auto &map = tag->frameListMap();
        const auto it = map.find("TXXX");
        if (it != map.end()) {
            const auto &frames = it->second;
            for (const auto &f : frames) {
                const auto frame = dynamic_cast<const TagLib::ID3v2::UserTextIdentificationFrame*>(f);
                if (!frame)
                    continue;
                if (frame->description().upper() == RG_STRING_UPPER[static_cast<int>(RGTag::TRACK_GAIN)])
                    return true;
            }
        }
    }
    return false;
}

template<typename T>
static bool tag_exists_xiph(const ScanJob::Track &track)
{
    bool ret = false;
    const TagLib::Ogg::XiphComment *tag = nullptr;
    T file(track.path.string().c_str(), false);
    if constexpr(std::is_same_v<T, TagLib::FLAC::File>)
        tag = file.xiphComment();
    else
        tag = dynamic_cast<TagLib::Ogg::XiphComment*>(file.tag());
    if (tag) {
        ret = tag->contains(RG_STRING_UPPER[static_cast<int>(RGTag::TRACK_GAIN)]);
        if constexpr(std::is_same_v<T, TagLib::Ogg::Opus::File>) {
            if (!ret)
                ret = tag->contains(R128_STRING[static_cast<int>(R128Tag::TRACK_GAIN)]);
        }
    }
    return ret;
}

static bool tag_exists_mp4(const ScanJob::Track &track)
{
    // Build static vector of upper and lowercase RG tags with iTunes atom
    static std::vector<TagLib::String> keys;
    if (keys.empty()) {
        keys.resize(2);
        const TagLib::String tags[] = {
            RG_STRING_UPPER[static_cast<int>(RGTag::TRACK_GAIN)],
            RG_STRING_LOWER[static_cast<int>(RGTag::TRACK_GAIN)]
        };
        for (auto &key : keys) {
            key = MP4_ATOM_STRING;
            key += tags[&key - &keys[0]];
        }
    }

    TagLib::MP4::File file(track.path.string().c_str(), false);
    const TagLib::MP4::Tag *tag = file.tag();
    if (tag) {
        for (const auto &key : keys) {
            if (tag->contains(key))
                return true;
        }
    }
    return false;
}

template<typename T>
static bool tag_exists_ape(const ScanJob::Track &track)
{
    T file(track.path.string().c_str(), false);
    const TagLib::APE::Tag *tag = file.APETag();
    if (tag) {
        const auto &map = tag->itemListMap();
        return map.contains(RG_STRING_UPPER[static_cast<int>(RGTag::TRACK_GAIN)]);
    }
    return false;
}

static bool tag_exists_asf(const ScanJob::Track &track)
{
    TagLib::ASF::File file(track.path.string().c_str(), false);
    const TagLib::ASF::Tag *tag = file.tag();
    return tag->contains(RG_STRING_UPPER[static_cast<int>(RGTag::TRACK_GAIN)]) ||
    tag->contains(RG_STRING_LOWER[static_cast<int>(RGTag::TRACK_GAIN)]);
}

template<typename T>
static void write_rg_tags(const ScanResult &result, const Config &config, T&& write_tag)
{
    write_tag(RGTag::TRACK_GAIN, FORMAT_GAIN(result.track_gain));
    write_tag(RGTag::TRACK_PEAK, FORMAT_PEAK(result.track_peak));
    if (config.do_album) {
        write_tag(RGTag::ALBUM_GAIN, FORMAT_GAIN(result.album_gain));
        write_tag(RGTag::ALBUM_PEAK, FORMAT_PEAK(result.album_peak));
    }
}

static bool tag_mp3(ScanJob::Track &track, const Config &config)
{
    TagLib::MPEG::File file(track.path.string().c_str());
    TagLib::ID3v2::Tag *tag = file.ID3v2Tag(true);
    if (!tag)
        return false;
    unsigned int id3v2version = config.id3v2version;
    if (id3v2version == ID3V2_KEEP)
        id3v2version = tag->isEmpty() ? 3: tag->header()->majorVersion();
    tag_clear(tag);
    if (config.tag_mode == 'i')
        tag_write(tag, track.result, config);

#if TAGLIB_VERSION < 11200
    return file.save(TagLib::MPEG::File::ID3v2, false, id3v2version);
#else
    return file.save(TagLib::MPEG::File::ID3v2, 
        TagLib::File::StripTags::StripNone,
        id3v2version == 3 ? TagLib::ID3v2::Version::v3 : TagLib::ID3v2::Version::v4
    );
#endif
}

static bool tag_flac(ScanJob::Track &track, const Config &config) 
{
    TagLib::FLAC::File file(track.path.string().c_str());
    TagLib::Ogg::XiphComment *tag = file.xiphComment(true);
    if (!tag)
        return false;
    tag_clear<TagLib::FLAC::File>(tag);
    if (config.tag_mode == 'i')
        tag_write<TagLib::FLAC::File>(tag, track.result, config);
    return file.save();
}

template<typename T>
static bool tag_ogg(ScanJob::Track &track, const Config &config) {
    {
        T file(track.path.string().c_str());
        TagLib::Ogg::XiphComment* tag = nullptr;
        if constexpr (std::is_same_v<T, TagLib::FileRef>)
            tag = dynamic_cast<TagLib::Ogg::XiphComment*>(file.tag());
        else
            tag = file.tag();
        if (!tag)
            return false;
        tag_clear<T>(tag);
        if (config.tag_mode == 'i' && (!std::is_same_v<T, TagLib::Ogg::Opus::File> ||
            (config.opus_mode != 't' && config.opus_mode != 'a')))
            tag_write<T>(tag, track.result, config);

        bool ret = file.save();
        if (!std::is_same_v<T, TagLib::Ogg::Opus::File> || config.tag_mode == 's' ||
            !(config.opus_mode == 't' || config.opus_mode == 'a') || !ret)
            return ret;

    }
    int16_t gain = config.opus_mode == 'a' && config.do_album ? 
    GAIN_TO_Q78(track.result.album_gain) : GAIN_TO_Q78(track.result.track_gain);
    return set_opus_header_gain(track.path.string().c_str(), gain);
}

static bool tag_mp4(ScanJob::Track &track, const Config &config)
{
    TagLib::MP4::File file(track.path.string().c_str());
    TagLib::MP4::Tag *tag = file.tag();
    if (!tag)
        return false;
    tag_clear(tag);
    if (config.tag_mode == 'i')
        tag_write(tag, track.result, config);
    
    return file.save();
}

template <typename T>
static bool tag_apev2(ScanJob::Track &track, const Config &config)
{
    T file(track.path.string().c_str());
    TagLib::APE::Tag *tag = file.APETag(true);
    if (!tag)
        return false;
    tag_clear(tag);
    if (config.tag_mode == 'i')
        tag_write(tag, track.result, config);
    if constexpr(!std::is_same_v<T, TagLib::MPC::File>)
        return file.save();
    else {
        bool ret = file.save();
        if (ret)
            ret = set_mpc_packet_rg(track.path.string().c_str());
        return ret;
    }
}

static bool tag_wma(ScanJob::Track &track, const Config &config)
{
    TagLib::ASF::File file(track.path.string().c_str());
    TagLib::ASF::Tag *tag = file.tag();
    if (!tag)
        return false;
    tag_clear(tag);
    if (config.tag_mode == 'i')
        tag_write(tag, track.result, config);

    return file.save();
}

template<typename T>
static bool tag_riff(ScanJob::Track &track, const Config &config)
{
    T file(track.path.string().c_str());
    TagLib::ID3v2::Tag *tag = nullptr;
    if constexpr (std::is_same_v<T, TagLib::RIFF::WAV::File>)
        tag = file.ID3v2Tag();
    else if constexpr (std::is_same_v<T, TagLib::RIFF::AIFF::File>)
        tag = file.tag();
    if (!tag)
        return false;
    unsigned int id3v2version = config.id3v2version;
    if (id3v2version == ID3V2_KEEP)
        id3v2version = tag->isEmpty() ? 3: tag->header()->majorVersion();
    tag_clear(tag);
    if (config.tag_mode == 'i')
        tag_write(tag, track.result, config);

    if constexpr (std::is_same_v<T, TagLib::RIFF::WAV::File>)
#if TAGLIB_VERSION < 11200
        return file.save(T::AllTags, false, id3v2version);
#else
        return file.save(T::AllTags,
            TagLib::File::StripTags::StripNone,
            id3v2version == 3 ? TagLib::ID3v2::Version::v3 : TagLib::ID3v2::Version::v4
        );
#endif
    else if constexpr (std::is_same_v<T, TagLib::RIFF::AIFF::File>) 
        return file.save();
}

template<int flags, typename T>
static void tag_clear_map(T&& clear)
{
    if constexpr((flags) & RG_TAGS_UPPERCASE) {
        for (const auto &tag : RG_STRING_UPPER)
            clear(tag);
    }
    if constexpr((flags) & RG_TAGS_LOWERCASE) {
        for (const auto &tag : RG_STRING_LOWER)
            clear(tag);
    }
    if constexpr((flags) & R128_TAGS) {
        for (const auto &tag : R128_STRING)
            clear(tag);
    }
}

static void tag_clear(TagLib::ID3v2::Tag *tag)
{
    const auto &map = tag->frameListMap();
    const auto it = map.find("TXXX");
    if (it == map.end())
        return; 
    TagLib::ID3v2::FrameList txxx_frames = it->second;
    for (auto &f : txxx_frames) {
        auto frame = dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame*>(f);
        if (frame && frame->fieldList().size() >= 2) {
            TagLib::String desc = frame->description().upper();
            auto rg_tag = std::find_if(RG_STRING_UPPER.begin(),
                              RG_STRING_UPPER.end(),
                              [&](const auto &tag_type) { return desc == tag_type; }
                          );
            if (rg_tag != RG_STRING_UPPER.end())
                tag->removeFrame(frame);
        }
    }

    // Also remove legacy RVAD (ID3v2.3) and RVA2 (ID3v2.4) which conflict with ReplayGain
    static const TagLib::ByteVector legacy_frames[] = {"RVAD", "RVA2"};
    for (const auto &frame_id : legacy_frames) {
        const auto it2 = map.find(frame_id);
        if (it2 != map.end()) {
            TagLib::ID3v2::FrameList frames = it2->second;
            for (auto frame : frames)
                tag->removeFrame(frame);
        }
    }
}

static void tag_write(TagLib::ID3v2::Tag *tag, const ScanResult &result, const Config &config)
{
    const RGTagsArray &RG_STRING = config.lowercase ? RG_STRING_LOWER : RG_STRING_UPPER;
    write_rg_tags(result,
        config,
        [&](RGTag rg_tag, const TagLib::String &value) {
            auto frame = new TagLib::ID3v2::UserTextIdentificationFrame();
            frame->setDescription(RG_STRING[static_cast<size_t>(rg_tag)]);
            frame->setText(value);
            tag->addFrame(frame);
        }
    );
}

template<typename T>
static void tag_clear(TagLib::Ogg::XiphComment *tag)
{   
    if constexpr(std::is_same_v<T, TagLib::Ogg::Opus::File>) {
        tag_clear_map<RG_TAGS_UPPERCASE | R128_TAGS>(
            [&](const TagLib::String &t) {
                tag->removeFields(t);
            }
        );
    }
    else {
        tag_clear_map<RG_TAGS_UPPERCASE>(
            [&](const TagLib::String &t) {
                tag->removeFields(t);
            }
        );
    }
}

template<typename T>
static void tag_write(TagLib::Ogg::XiphComment *tag, const ScanResult &result, const Config &config)
{
    const RGTagsArray &RG_STRING = RG_STRING_UPPER;

    // Opus RFC 7845 tag
    if (std::is_same_v<T, TagLib::Ogg::Opus::File> && (config.opus_mode == 'r' || config.opus_mode == 's')) {
        tag->addField(R128_STRING[static_cast<int>(R128Tag::TRACK_GAIN)], 
            rsgain::format("{}", GAIN_TO_Q78(result.track_gain))
        );

        if (config.do_album) {
            tag->addField(R128_STRING[static_cast<int>(R128Tag::ALBUM_GAIN)], 
                rsgain::format("{}", GAIN_TO_Q78(result.album_gain))
            );
        }
    }

    // Default ReplayGain tag
    else {
        write_rg_tags(result,
            config,
            [&](RGTag rg_tag, const TagLib::String &value) {
                tag->addField(RG_STRING[static_cast<size_t>(rg_tag)], value);
            }
        );
    }
}

static void tag_clear(TagLib::MP4::Tag *tag)
{
    tag_clear_map<RG_TAGS_UPPERCASE | RG_TAGS_LOWERCASE>(
        [&](const TagLib::String &t) {
            TagLib::String tag_name;
            FORMAT_MP4_TAG(tag_name, t);
            tag->removeItem(tag_name);
        }
    );
}

static void tag_write(TagLib::MP4::Tag *tag, const ScanResult &result, const Config &config) 
{
    const RGTagsArray &RG_STRING = config.lowercase ? RG_STRING_LOWER : RG_STRING_UPPER;
    write_rg_tags(result,
        config,
        [&](RGTag rg_tag, const TagLib::String &value) {
            TagLib::String tag_name;
            FORMAT_MP4_TAG(tag_name, RG_STRING[static_cast<size_t>(rg_tag)]);
            tag->setItem(tag_name, TagLib::MP4::Item(value));
        }
    );
}

static void tag_clear(TagLib::APE::Tag *tag)
{
    tag_clear_map<RG_TAGS_UPPERCASE>(
        [&](const TagLib::String &t) {
            tag->removeItem(t);
        }
    );
}

static void tag_write(TagLib::APE::Tag *tag, const ScanResult &result, const Config &config)
{
    const RGTagsArray &RG_STRING = RG_STRING_UPPER;
    write_rg_tags(result,
        config,
        [&](RGTag rg_tag, const TagLib::String &value) {
            tag->addValue(RG_STRING[static_cast<size_t>(rg_tag)], value);
        }
    );
}

static void tag_clear(TagLib::ASF::Tag *tag) 
{
    tag_clear_map<RG_TAGS_UPPERCASE | RG_TAGS_LOWERCASE>(
        [&](const TagLib::String &t) {
            tag->removeItem(t);
        }
    );
}

static void tag_write(TagLib::ASF::Tag *tag, const ScanResult &result, const Config &config)
{
    const RGTagsArray &RG_STRING = config.lowercase ? RG_STRING_LOWER : RG_STRING_UPPER;
    write_rg_tags(result,
        config,
        [&](RGTag rg_tag, const TagLib::String &value) {
            tag->setAttribute(RG_STRING[static_cast<size_t>(rg_tag)], value);
        }
    );
}

static_assert(-1 == ~0); // 2's complement for signed integers
bool set_opus_header_gain(const char* path, int16_t gain)
{
    uint32_t crc;
    if constexpr (std::endian::native == std::endian::big)
        gain = static_cast<int16_t>((gain << 8) & 0xff00) | ((gain >> 8) & 0x00ff);

    std::unique_ptr<std::FILE, int (*)(FILE*)> file(fopen(path, "rb+"), fclose);
    if (!file)
        return false;

    char buffer[8];
    size_t page_size = 0;
    size_t opus_header_size = 0;

    // Check for OggS header
    if (fseek(file.get(), 0, SEEK_SET)
        || fread(buffer, 1, 4, file.get()) != 4
        || strncmp(buffer, "OggS", 4))
        return false;
    
    // Check for OpusHead header
    if (fseek(file.get(), OPUS_HEAD_OFFSET, SEEK_SET)
        || fread(buffer, 1, 8, file.get()) != 8
        || strncmp(buffer, "OpusHead", 8))
        return false;

    // Read the size of the Opus header
    if (fseek(file.get(), OGG_SEGMENT_TABLE_OFFSET, SEEK_SET)
        || fread((void*) &opus_header_size, 1, 1, file.get()) != 1)
        return false;
    page_size = OPUS_HEAD_OFFSET + opus_header_size;

    // To verify the page size, make sure the next Ogg page is where we expect it
    if (fseek(file.get(), page_size, SEEK_SET)
        || fread(buffer, 1, 4, file.get()) != 4
        || strncmp(buffer, "OggS", 4))
        return false;

    // Read the entire Ogg page into memory
    auto page = std::make_unique<char[]>(page_size);
    if (fseek(file.get(), 0, SEEK_SET)
        || fread(page.get(), 1, page_size, file.get()) != page_size)
        return false;

    // Clear CRC, set gain
    memset(page.get() + OGG_CRC_OFFSET, 0, sizeof(crc));
    memcpy(page.get() + OPUS_GAIN_OFFSET, &gain, sizeof(gain));

    // Calculate new CRC
    static const CRC::Table<uint32_t, 32> table({0x04C11DB7, 0, 0, false, false});
    crc = CRC::Calculate(page.get(), page_size, table);

    // Write new CRC and gain to file
    fseek(file.get(), OGG_CRC_OFFSET, SEEK_SET);
    fwrite(&crc, sizeof(crc), 1, file.get());
    fseek(file.get(), OPUS_GAIN_OFFSET, SEEK_SET);
    fwrite(&gain, sizeof(gain), 1, file.get());
    return true;
}

static bool set_mpc_packet_rg(const char *path)
{
    std::FILE *fp = fopen(path, "rb+");
    if (fp == nullptr)
        return false;
    std::unique_ptr<std::FILE, int (*)(FILE*)> file(fp, fclose);

    fseek(fp, 0L, SEEK_END);
    size_t nb_bytes = static_cast<size_t>(ftell(fp));
    rewind(fp);

    // Validate magic number
    char magic_num[4];
    if (fread(magic_num, 1, sizeof(magic_num), fp) != sizeof(magic_num) 
    || strncmp(magic_num, "MPCK", sizeof(magic_num)))
        return false;
    nb_bytes -= sizeof(magic_num);

    // Loop through all the packets until we find "RG"
    char key[2];
    unsigned char length_buffer[4];
    unsigned int length_bytes; // Tracks width of length buffer (1-4)
    uint32_t length;
    size_t total_bytes_read = 0;
    size_t payload_bytes;
    while (total_bytes_read < nb_bytes) {
        total_bytes_read += fread(key, 1, sizeof(key), fp);
        
        // Find length of the packet
        length_bytes = 0;
        length = 0;
        do {
            total_bytes_read += fread(length_buffer + length_bytes, 1, 1, fp);
            length_bytes++;
        } while ((length_buffer[length_bytes - 1] & 0x80) && total_bytes_read < nb_bytes && length_bytes < 4);
        for (size_t i = 0; i < length_bytes; i++)
            length += (uint32_t) (0x7F & length_buffer[i]) << (7 * (length_bytes - i - 1));
        payload_bytes = length - (2 + length_bytes);

        // Clear the ReplayGain info
        if (!strncmp(key, "RG", 2) && length == 12) {
            static char rg_buffer[] = {
                0x1, // version
                0x0, 0x0, // track gain
                0x0, 0x0, // track peak
                0x0, 0x0, // album gain
                0x0, 0x0, // album peak
            };
            fwrite(rg_buffer, 1, sizeof(rg_buffer), fp);
            return true;
        }
        total_bytes_read += payload_bytes;
        fseek(fp, static_cast<long>(payload_bytes), SEEK_CUR);
    }
    return false;
}
