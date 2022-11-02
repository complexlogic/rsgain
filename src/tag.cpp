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

#include <taglib.h>
#include <fileref.h>
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
#include <wavfile.h>
#include <aifffile.h>
#include <wavpackfile.h>
#include <apefile.h>
#include <mpcfile.h>
#include <libavcodec/avcodec.h>

#define CRCPP_USE_CPP11
#include "external/CRC.h"

#include "rsgain.hpp"
#include "scan.hpp"
#include "tag.hpp"
#include "output.hpp"

template<typename T>
static void write_rg_tags(const ScanResult &result, const Config &config, T&& write_tag);
template<int flags, typename T>
static void tag_clear_map(T&& clear);
static void tag_clear_id3(TagLib::ID3v2::Tag *tag);
static void tag_write_id3(TagLib::ID3v2::Tag *tag, const ScanResult &result, const Config &config);
template<typename T>
static void tag_clear_xiph(TagLib::Ogg::XiphComment *tag);
template<typename T>
static void tag_write_xiph(TagLib::Ogg::XiphComment *tag, const ScanResult &result, const Config &config);
static void tag_clear_mp4(TagLib::MP4::Tag *tag);
static void tag_write_mp4(TagLib::MP4::Tag *tag, const ScanResult &result, const Config &config);
static void tag_clear_apev2(TagLib::APE::Tag *tag);
static void tag_write_apev2(TagLib::APE::Tag *tag, const ScanResult &result, const Config &config);
static void tag_clear_asf(TagLib::ASF::Tag *tag);
static void tag_write_asf(TagLib::ASF::Tag *tag, const ScanResult &result, const Config &config);

template<typename T>
static bool tag_exists_id3(const Track &track);
template<typename T>
static bool tag_exists_xiph(const Track &track);
static bool tag_exists_mp4(const Track &track);
template<typename T>
static bool tag_exists_ape(const Track &track);
static bool tag_exists_asf(const Track &track);

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
template<std::size_t N, class T>
constexpr std::size_t array_size(T(&)[N]) {return N;}
static_assert((size_t) RGTag::MAX_VAL == array_size(RG_STRING_UPPER));
static_assert(array_size(RG_STRING_UPPER) == array_size(RG_STRING_LOWER));

enum class R128Tag {
    TRACK_GAIN,
    ALBUM_GAIN,
    MAX_VAL
};

static const char *R128_STRING[] = {
    "R128_TRACK_GAIN",
    "R128_ALBUM_GAIN"
};
static_assert((size_t) R128Tag::MAX_VAL == array_size(R128_STRING));

void tag_track(Track &track, const Config &config)
{
    switch (track.type) {
        case FileType::MP2:
        case FileType::MP3:
            if (!tag_mp3(track, config))
                tag_error(track);
            break;

        case FileType::FLAC:
            if (!tag_flac(track, config))
                tag_error(track);
            break;

        case FileType::OGG:
            switch (track.codec_id) {
                case AV_CODEC_ID_OPUS:
                    if (!tag_ogg<TagLib::Ogg::Opus::File>(track, config))
                        tag_error(track);
                    break;

                case AV_CODEC_ID_VORBIS:
                    if (!tag_ogg<TagLib::Ogg::Vorbis::File>(track, config))
                        tag_error(track);
                    break;

                case AV_CODEC_ID_FLAC:
                    if (!tag_ogg<TagLib::Ogg::FLAC::File>(track, config))
                        tag_error(track);
                    break;

                case AV_CODEC_ID_SPEEX:
                    if (!tag_ogg<TagLib::Ogg::Speex::File>(track, config))
                        tag_error(track);
                    break;

                default:
                    if (!tag_ogg<TagLib::FileRef>(track, config))
                        tag_error(track);
                    break;
            }
            break;
                
        case FileType::OPUS:
            if (!tag_ogg<TagLib::Ogg::Opus::File>(track, config))
                tag_error(track);
            break;

        case FileType::M4A:
            if (!tag_mp4(track, config))
                tag_error(track);
            break;

        case FileType::WMA:
            if (!tag_wma(track, config))
                tag_error(track);
            break;

        case FileType::WAV:
            if (!tag_riff<TagLib::RIFF::WAV::File>(track, config))
                tag_error(track);
            break;

        case FileType::AIFF:
            if (!tag_riff<TagLib::RIFF::AIFF::File>(track, config))
                tag_error(track);
            break;

        case FileType::WAVPACK:
            if (!tag_apev2<TagLib::WavPack::File>(track, config))
                tag_error(track);
            break;

        case FileType::APE:
            if (!tag_apev2<TagLib::APE::File>(track, config))
                tag_error(track);
            break;

        case FileType::MPC:
            if (!tag_apev2<TagLib::MPC::File>(track, config))
                tag_error(track);
            break;
    }
}

bool tag_exists(const Track &track)
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
            return tag_exists_ape<TagLib::APE::File>(track);

        case FileType::MPC:
            return tag_exists_ape<TagLib::MPC::File>(track);

        default:
            return false;
    }
    return false;
}

template<typename T>
static bool tag_exists_id3(const Track &track)
{
    bool ret = false;
    TagLib::ID3v2::Tag *tag = nullptr;
    T file(track.path.c_str());
    if constexpr (std::is_same_v<T, TagLib::RIFF::AIFF::File>)
        tag = file.tag();
    else
        tag = file.ID3v2Tag();
    if (tag != nullptr) {
        auto frames = tag->frameList("TXXX");
        for (auto &f : frames) {
            TagLib::ID3v2::UserTextIdentificationFrame *frame = dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame*>(f);
            if (frame == nullptr)
                continue;
            TagLib::StringList string_list = frame->fieldList();
            if (string_list.size() < 2)
                continue;
            TagLib::String desc = frame->description().upper();
            if (desc == RG_STRING_UPPER[static_cast<int>(RGTag::TRACK_GAIN)]) {
                ret = true;
                break;
            }
        }
    }
    return ret;
}

template<typename T>
static bool tag_exists_xiph(const Track &track)
{
    bool ret = false;
    TagLib::Ogg::XiphComment *tag = nullptr;
    T file(track.path.c_str());
    if constexpr(std::is_same_v<T, TagLib::FLAC::File>)
        tag = file.xiphComment();
    else
        tag = dynamic_cast<TagLib::Ogg::XiphComment*>(file.tag());
    if (tag != nullptr)
        ret = tag->contains(RG_STRING_UPPER[static_cast<int>(RGTag::TRACK_GAIN)]);
        if constexpr(std::is_same_v<T, TagLib::Ogg::Opus::File>) {
            if (!ret)
                ret = tag->contains(R128_STRING[static_cast<int>(R128Tag::TRACK_GAIN)]);
        }
    return ret;
}

static bool tag_exists_mp4(const Track &track)
{
    TagLib::MP4::File file(track.path.c_str());
    TagLib::MP4::Tag *tag = file.tag();
    if (tag != nullptr) {
        const TagLib::MP4::ItemMap map = tag->itemMap();
        TagLib::String key(MP4_ATOM_STRING);
        key = key.upper();
        key += RG_STRING_UPPER[static_cast<int>(RGTag::TRACK_GAIN)];
        for (const auto &item : map) {
            if (item.first.upper() == key)
                return true;
        }
    }
    return false;
}

template<typename T>
static bool tag_exists_ape(const Track &track)
{
    T file(track.path.c_str());
    TagLib::APE::Tag *tag = file.APETag();
    if (tag != nullptr) {
        const TagLib::APE::ItemListMap map = tag->itemListMap();
        const char *rg_tag = RG_STRING_UPPER[static_cast<int>(RGTag::TRACK_GAIN)];
        for (const auto &item : map) {
            if (item.first.upper() == rg_tag)
                return true;
        }
    }
    return false;
}

static bool tag_exists_asf(const Track &track)
{
    TagLib::ASF::File file(track.path.c_str());
    TagLib::ASF::Tag *tag = file.tag();
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

static bool tag_mp3(Track &track, const Config &config)
{
    TagLib::MPEG::File file(track.path.c_str());
    TagLib::ID3v2::Tag *tag = file.ID3v2Tag(true);
    tag_clear_id3(tag);
    if (config.tag_mode == 'i')
        tag_write_id3(tag, track.result, config);

    // Using the deprecated calls until taglib 1.12+ becomes more widely adopted
    return file.save(TagLib::MPEG::File::ID3v2, false, config.id3v2version);
}

static bool tag_flac(Track &track, const Config &config) 
{
    TagLib::FLAC::File file(track.path.c_str());
    TagLib::Ogg::XiphComment *tag = file.xiphComment(true);
    tag_clear_xiph<TagLib::FLAC::File>(tag);
    if (config.tag_mode == 'i')
        tag_write_xiph<TagLib::FLAC::File>(tag, track.result, config);
    return file.save();
}

template<typename T>
static bool tag_ogg(Track &track, const Config &config) {
    T file(track.path.c_str());
    TagLib::Ogg::XiphComment *tag = nullptr;
    if constexpr(std::is_same_v<T, TagLib::FileRef>) {
        tag = dynamic_cast<TagLib::Ogg::XiphComment*>(file.tag());
        if (tag == nullptr)
            return false;
    }
    else
        tag = file.tag();
    tag_clear_xiph<T>(tag);
    if (config.tag_mode == 'i' && (!std::is_same_v<T, TagLib::Ogg::Opus::File> || 
    (config.opus_mode != 't' && config.opus_mode != 'a')))
        tag_write_xiph<T>(tag, track.result, config);

    bool ret = file.save();
    if (!std::is_same_v<T, TagLib::Ogg::Opus::File> || config.tag_mode == 's' || 
    config.opus_mode == 'd' || config.opus_mode == 'r' || !ret)
        return ret;

    int16_t gain = config.opus_mode == 'a' && config.do_album ? 
    GAIN_TO_Q78(track.result.album_gain) : GAIN_TO_Q78(track.result.track_gain);
    return set_opus_header_gain(track.path.c_str(), gain);
}

static bool tag_mp4(Track &track, const Config &config)
{
    TagLib::MP4::File file(track.path.c_str());
    TagLib::MP4::Tag *tag = file.tag();
    tag_clear_mp4(tag);
    if (config.tag_mode == 'i')
        tag_write_mp4(tag, track.result, config);
    
    return file.save();
}

template <typename T>
static bool tag_apev2(Track &track, const Config &config)
{
    T file(track.path.c_str());
    TagLib::APE::Tag *tag = file.APETag(true);
    tag_clear_apev2(tag);
    if (config.tag_mode == 'i')
        tag_write_apev2(tag, track.result, config);
    if constexpr(!std::is_same_v<T, TagLib::MPC::File>)
        return file.save();
    else {
        bool ret = file.save();
        if (ret)
            ret = set_mpc_packet_rg(track.path.c_str());
        return ret;
    }
}

static bool tag_wma(Track &track, const Config &config)
{
    TagLib::ASF::File file(track.path.c_str());
    TagLib::ASF::Tag *tag = file.tag();
    tag_clear_asf(tag);
    if (config.tag_mode == 'i')
        tag_write_asf(tag, track.result, config);

    return file.save();
}

template<typename T>
static bool tag_riff(Track &track, const Config &config)
{
    T file(track.path.c_str());
    TagLib::ID3v2::Tag *tag;
    if constexpr (std::is_same_v<T, TagLib::RIFF::WAV::File>)
        tag = file.ID3v2Tag();
    else if constexpr (std::is_same_v<T, TagLib::RIFF::AIFF::File>)
        tag = file.tag();
    tag_clear_id3(tag);
    if (config.tag_mode == 'i')
        tag_write_id3(tag, track.result, config);

    if constexpr (std::is_same_v<T, TagLib::RIFF::WAV::File>)
        return file.save(T::AllTags, false, config.id3v2version);
    else if constexpr (std::is_same_v<T, TagLib::RIFF::AIFF::File>) 
        return file.save();
}

template<int flags, typename T>
static void tag_clear_map(T&& clear)
{
    if constexpr((flags) & RG_TAGS_UPPERCASE) {
        for (const char *tag : RG_STRING_UPPER)
            clear(tag);
    }
    if constexpr((flags) & RG_TAGS_LOWERCASE) {
        for (const char *tag : RG_STRING_LOWER)
            clear(tag);
    }
    if constexpr((flags) & R128_TAGS) {
        for (const char *tag : R128_STRING)
            clear(tag);
    }
}

static void tag_clear_id3(TagLib::ID3v2::Tag *tag)
{
    TagLib::ID3v2::FrameList frames = tag->frameList("TXXX");
    for (auto &f : frames) {
        TagLib::ID3v2::UserTextIdentificationFrame *frame =
        dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame*>(f);
        if (frame != nullptr && frame->fieldList().size() >= 2) {
            TagLib::String desc = frame->description().upper();
            auto rg_tag = std::find_if(std::cbegin(RG_STRING_UPPER),
                              std::cend(RG_STRING_UPPER),
                              [&](const auto &tag_type) {return desc == tag_type;}
                          );
            if (rg_tag != std::cend(RG_STRING_UPPER))
                tag->removeFrame(frame);
        }
    }
}

static void tag_write_id3(TagLib::ID3v2::Tag *tag, const ScanResult &result, const Config &config)
{
    const char **RG_STRING = config.lowercase ? RG_STRING_LOWER : RG_STRING_UPPER;
    write_rg_tags(result,
        config,
        [&](RGTag rg_tag, const std::string &value) {
            TagLib::ID3v2::UserTextIdentificationFrame *frame = new TagLib::ID3v2::UserTextIdentificationFrame;
            frame->setDescription(RG_STRING[static_cast<int>(rg_tag)]);
            frame->setText(value);
            tag->addFrame(frame);
        }
    );
}

template<typename T>
static void tag_clear_xiph(TagLib::Ogg::XiphComment *tag)
{   
    if constexpr(std::is_same_v<T, TagLib::Ogg::Opus::File>) {
        tag_clear_map<RG_TAGS_UPPERCASE | R128_TAGS>(
            [&](const char *t) {
                tag->removeFields(t);
            }
        );
    }
    else {
        tag_clear_map<RG_TAGS_UPPERCASE>(
            [&](const char *t) {
                tag->removeFields(t);
            }
        );
    }
}

template<typename T>
static void tag_write_xiph(TagLib::Ogg::XiphComment *tag, const ScanResult &result, const Config &config)
{
    static const char **RG_STRING = RG_STRING_UPPER;

    // Opus RFC 7845 tag
    if (std::is_same_v<T, TagLib::Ogg::Opus::File> && config.opus_mode == 'r') {
        tag->addField(R128_STRING[static_cast<int>(R128Tag::TRACK_GAIN)], 
            fmt::format("{}", GAIN_TO_Q78(result.track_gain))
        );

        if (config.do_album) {
            tag->addField(R128_STRING[static_cast<int>(R128Tag::ALBUM_GAIN)], 
                fmt::format("{}", GAIN_TO_Q78(result.album_gain))
            );
        }
    }

    // Default ReplayGain tag
    else {
        write_rg_tags(result,
            config,
            [&](RGTag rg_tag, const std::string &value) {
                tag->addField(RG_STRING[static_cast<int>(rg_tag)], value);
            }
        );
    }
}

static void tag_clear_mp4(TagLib::MP4::Tag *tag)
{
    tag_clear_map<RG_TAGS_UPPERCASE | RG_TAGS_LOWERCASE>(
        [&](const char *t) {
            TagLib::String tag_name;
            FORMAT_MP4_TAG(tag_name, t);
            tag->removeItem(tag_name);
        }
    );
}

static void tag_write_mp4(TagLib::MP4::Tag *tag, const ScanResult &result, const Config &config) 
{
    const char **RG_STRING = config.lowercase ? RG_STRING_LOWER : RG_STRING_UPPER;
    write_rg_tags(result,
        config,
        [&](RGTag rg_tag, const std::string &value) {
            TagLib::String tag_name;
            FORMAT_MP4_TAG(tag_name, RG_STRING[static_cast<int>(rg_tag)]);
            tag->setItem(tag_name, TagLib::StringList(value));
        }
    );
}

static void tag_clear_apev2(TagLib::APE::Tag *tag)
{
    tag_clear_map<RG_TAGS_UPPERCASE | RG_TAGS_LOWERCASE>(
        [&](const char *t) {
            tag->removeItem(t);
        }
    );
}

static void tag_write_apev2(TagLib::APE::Tag *tag, const ScanResult &result, const Config &config)
{
    static const char **RG_STRING = RG_STRING_UPPER;
    write_rg_tags(result,
        config,
        [&](RGTag rg_tag, const std::string &value) {
            tag->addValue(RG_STRING[static_cast<int>(rg_tag)], TagLib::String(value));
        }
    );
}

static void tag_clear_asf(TagLib::ASF::Tag *tag) 
{
    tag_clear_map<RG_TAGS_UPPERCASE | RG_TAGS_LOWERCASE>(
        [&](const char *t) {
            tag->removeItem(t);
        }
    );
}

static void tag_write_asf(TagLib::ASF::Tag *tag, const ScanResult &result, const Config &config)
{
    const char **RG_STRING = config.lowercase ? RG_STRING_LOWER : RG_STRING_UPPER;
    write_rg_tags(result,
        config,
        [&](RGTag rg_tag, const std::string &value) {
            tag->setAttribute(RG_STRING[static_cast<int>(rg_tag)], TagLib::String(value));
        }
    );
}

static_assert(-1 == ~0); // 2's complement for signed integers
bool set_opus_header_gain(const char *path, int16_t gain)
{   
    char buffer[OPUS_HEADER_SIZE]; // 47 bytes
    uint32_t crc;
    if constexpr(std::endian::native == std::endian::big)
        gain = ((gain << 8) & 0xff00) | ((gain >> 8) & 0x00ff);
    
    // Read header into memory
    std::unique_ptr<std::FILE, decltype(&fclose)> file(fopen(path, "rb+"), fclose);
    size_t read = fread(buffer, 1, sizeof(buffer), file.get());
    
    // Make sure we have a valid Ogg/Opus header
    if (read != sizeof(buffer) || strncmp(buffer, "OggS", 4)  ||
    strncmp(buffer + OPUS_HEAD_OFFSET, "OpusHead", 8))  
        return false;

    // Clear CRC, set gain
    memset(buffer + OGG_CRC_OFFSET, 0, sizeof(crc));
    memcpy(buffer + OPUS_GAIN_OFFSET, &gain, sizeof(gain));
    
    // Calculate new CRC
    static const CRC::Table<uint32_t, 32> table({0x04C11DB7, 0, 0, false, false});
    crc = CRC::Calculate(buffer, sizeof(buffer), table);
    
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
    std::unique_ptr<std::FILE, decltype(&fclose)> file(fopen(path, "rb+"), fclose);

    fseek(fp, 0L, SEEK_END);
    size_t nb_bytes = ftell(fp);
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
        for (int i = 0; i < length_bytes; i++)
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
        fseek(fp, payload_bytes, SEEK_CUR);
    }
    return false;
}

void taglib_get_version(std::string &buffer)
{
    buffer = fmt::format("{}.{}.{}", TAGLIB_MAJOR_VERSION, TAGLIB_MINOR_VERSION, TAGLIB_PATCH_VERSION);
}
