#define UNIT_DB "dB"
#define UNIT_LU "LU"
#define CMD_HELP(CMDL, CMDS, MSG) fmt::print("  {}{:<5} {:<16}{}  {}.\n", COLOR_YELLOW, CMDS ",", CMDL, COLOR_OFF, MSG);
#define CMD_CMD(CMD, MSG) fmt::print("  {}{:<22}{}  {}.\n", COLOR_YELLOW, CMD, COLOR_OFF, MSG);
#define CMD_CONT(MSG) fmt::print("  {}{:<5} {:<16}{}  {}.\n", COLOR_YELLOW, "", "", COLOR_OFF, MSG);
#define PRINT_LIB(lib, version) fmt::print("  " COLOR_YELLOW " {:<13}" COLOR_OFF " {}\n", lib, version);

#define MATCH(x,y) !strcmp(x,y)

typedef struct {
	char mode;
	const char *unit;
	double pre_gain;
	double max_true_peak_level;
	bool no_clip;
	bool warn_clip;
	bool do_album;
	bool tab_output;
	bool lowercase;
	bool strip;
	int id3v2version;
} Config;


#ifdef __cplusplus
extern "C" {
#endif
void quit(int status);
void parse_pregain(const char *value, Config *config);
void parse_mode(const char *value, Config *config);
void parse_id3v2version(const char *value, Config *config);
void parse_max_true_peak_level(const char *value, Config *config);
#ifdef __cplusplus
}
#endif