#pragma once

#include <mutex>
#include <vector>
#include <filesystem>
#include <ebur128.h>

void free_ebur128(ebur128_state *ebur128);

enum class FileType {
    INVALID = -1,
    DEFAULT,
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
    APE,
	TAK,
	MPC
};

struct ScanResult {
	double track_gain;
	double track_peak;
	double track_loudness;

	double album_gain;
	double album_peak;
	double album_loudness;
};

struct ScanData {
    size_t files = 0;
	size_t skipped = 0;
    size_t clipping_adjustments = 0;
    double total_gain = 0.0;
    double total_peak = 0.0;
    size_t total_negative = 0;
    size_t total_positive = 0;
    std::vector<std::string> error_directories;
};


class ScanJob {
	public:
		struct Track {
			std::filesystem::path path;
			FileType type;
			std::unique_ptr<ebur128_state, decltype(&free_ebur128)> ebur128;
			std::string container;
			ScanResult result;
			int codec_id;
			bool tclip = false;
			bool aclip = false;

			Track(const std::filesystem::path &path, FileType type) : path(path), type(type), ebur128(nullptr, free_ebur128) {};
			bool scan(const Config &config, std::mutex *ffmpeg_mutex);
			void calculate_loudness(const Config &config);
		};

		std::filesystem::path path;
		size_t nb_files;
		const Config &config;
		FileType type;
		bool error = false;
		size_t clipping_adjustments = 0;
		size_t skipped = 0;

		ScanJob(const std::filesystem::path &path, std::vector<Track> &tracks, const Config &config, FileType &type) : path(path), nb_files(tracks.size()), config(config), type(type), tracks(std::move(tracks)) {}
		ScanJob(std::vector<Track> &tracks, const Config &config, FileType type) : nb_files(tracks.size()), config(config), type(type), tracks(std::move(tracks)) {}
		static ScanJob* factory(char **files, size_t nb_files, const Config &config);
		static ScanJob* factory(const std::filesystem::path &path);
		bool scan(std::mutex *ffmpeg_mutex = nullptr);
		void update_data(ScanData &data);

	private:
		std::vector<Track> tracks;

		void calculate_loudness();
		void calculate_album_loudness();
		void tag_tracks();
};
