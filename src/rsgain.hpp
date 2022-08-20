#define CMD_HELP(CMDL, CMDS, MSG) fmt::print("  {}{:<5} {:<18}{}  {}.\n", COLOR_YELLOW, CMDS ",", CMDL, COLOR_OFF, MSG);
#define CMD_CMD(CMD, MSG) fmt::print("  {}{:<22}{}  {}.\n", COLOR_YELLOW, CMD, COLOR_OFF, MSG);
#define CMD_CONT(MSG) fmt::print("  {}{:<5} {:<18}{}  {}.\n", COLOR_YELLOW, "", "", COLOR_OFF, MSG);
#define PRINT_LIB(lib, version) fmt::print("  " COLOR_YELLOW " {:<13}" COLOR_OFF " {}\n", lib, version);

#define MATCH(x,y) !strcmp(x,y)

#define MAX_TARGET_LOUDNESS -5
#define MIN_TARGET_LOUDNESS -30

#define RG_TARGET_LOUDNESS -18.f
#define EBU_R128_MAX_PEAK -1.f

typedef struct {
	char tag_mode;
	double target_loudness;
	double max_peak_level;
	bool true_peak;
	char clip_mode;
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
void parse_target_loudness(const char *value, Config &config);
void parse_tag_mode(const char *value, Config &config);
bool parse_clip_mode(const char *value, char &mode);
void parse_id3v2_version(const char *value, Config &config);
void parse_max_peak_level(const char *value, Config &config);
#ifdef __cplusplus
}
#endif