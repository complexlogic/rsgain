#define UNIT_DB "dB"
#define UNIT_LU "LU"
#define CMD_HELP(CMDL, CMDS, MSG) output("  %s%-5s %-16s%s  %s.\n", COLOR_YELLOW, CMDS ",", CMDL, COLOR_OFF, MSG);
#define CMD_CMD(CMD, MSG) output("  %s%-22s%s  %s.\n", COLOR_YELLOW, CMD, COLOR_OFF, MSG);
#define CMD_CONT(MSG) output("  %s%-5s %-16s%s  %s.\n", COLOR_YELLOW, "", "", COLOR_OFF, MSG);

typedef struct {
	char mode;
	const char *unit;
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

#ifdef __cplusplus
extern "C" {
#endif
void quit(int status);
void scan(int nb_files, char *files[], Config *config);
#ifdef __cplusplus
}
#endif