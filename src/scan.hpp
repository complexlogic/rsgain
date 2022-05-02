#include <mutex>
#include "scan.h"

typedef struct {
	char *file;
	char *container;
	int   codec_id;

	double track_gain;
	double track_peak;

	double track_loudness;
	double track_loudness_range;

	double album_gain;
	double album_peak;

	double album_loudness;
	double album_loudness_range;

	double loudness_reference;
} scan_result;

enum AV_CONTAINER_ID {
    AV_CONTAINER_ID_MP3,
		AV_CONTAINER_ID_FLAC,
		AV_CONTAINER_ID_OGG,
		AV_CONTAINER_ID_MP4,
		AV_CONTAINER_ID_ASF,
		AV_CONTAINER_ID_WAV,
		AV_CONTAINER_ID_WV,
		AV_CONTAINER_ID_AIFF,
		AV_CONTAINER_ID_APE
};

int name_to_id(const char *str);
int scan_init(unsigned nb_files);
void scan_deinit();
bool scan_file(const char *file, unsigned index, std::mutex *m);
void apply_gain(int nb_files, char *files[], Config *config);
scan_result *scan_get_track_result(unsigned index, double pre_gain);
double scan_get_album_peak();
void scan_set_album_result(scan_result *result, double pre_amp);
int scan_album_has_different_codecs(void);
int scan_album_has_different_containers(void);
int scan_album_has_opus(void);