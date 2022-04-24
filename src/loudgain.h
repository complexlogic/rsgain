#define UNIT_DB "dB"
#define UNIT_LU "LU"

typedef struct {
	char mode;
	char *unit;
	double pre_gain;
	double max_true_peak_level;
	bool no_clip;
	bool warn_clip;
	bool do_album;
	bool tab_output;
	bool tab_output_new;
	bool lowercase;
	bool strip;
	int id3v2version;
} Config;