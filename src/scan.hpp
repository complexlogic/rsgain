#ifndef HAS_SCAN_H
#define HAS_SCAN_H

#include <mutex>
#include <vector>
#include <filesystem>
#include <ebur128.h>
#include "scan.hpp"

#define OUTPUT_FORMAT AV_SAMPLE_FMT_S16

typedef enum {
    INVALID = -1,
    MP2,
    MP3,
    FLAC,
    OGG,
    OPUS,
    M4A,
    WMA,
    WAV,
    AIFF,
    WAVPACK,
    APE
} FileType;

struct extension_type {
    const char *extension;
    FileType file_type;
};

typedef struct ScanResult {
	double track_gain;
	double track_peak;
	double track_loudness;

	double album_gain;
	double album_peak;
	double album_loudness;
} ScanResult;

typedef struct ScanData {
    unsigned int files;
    unsigned int clipping_adjustments;
    double total_gain;
    double total_peak;
    unsigned int total_negative;
    unsigned int total_positive;
    std::vector<std::string> error_directories;

    ScanData(void) : files(0), clipping_adjustments(0), total_gain(0.f), total_peak(0.f), total_positive(0), total_negative(0){};
} ScanData;

typedef struct Track{
	std::string path;
	FileType type;
	std::string container;
	ScanResult result;
	int codec_id;
	ebur128_state *ebur128;
	bool tclip;
	bool aclip;

	Track(const std::string &path, FileType type) : path(path), type(type), ebur128(NULL), tclip(false), aclip(false) {};
	~Track();
	bool scan(const Config &config, std::mutex *ffmpeg_mutex);
	int calculate_loudness(const Config &config);
} Track;

class ScanJob {
	private:
		std::vector<Track> tracks;

		void calculate_loudness(const Config &config);
		void calculate_album_loudness(const Config &config);
		void tag_tracks(const Config &config);

	public:
		FileType type;
		int nb_files;
		std::string path;
		bool error;
		int clipping_adjustments;

		ScanJob() : nb_files(0), error(false), clipping_adjustments(0) {};
		int add_files(char **files, int nb_files);
		FileType add_directory(std::filesystem::path &path);
		bool scan(const Config &config, std::mutex *ffmpeg_mutex = NULL);
		void update_data(ScanData &data);
};

#endif