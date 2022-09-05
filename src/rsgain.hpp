#define CMD_HELP(CMDL, CMDS, MSG) fmt::print("  {}{:<5} {:<18}{}  {}.\n", COLOR_YELLOW, CMDS ",", CMDL, COLOR_OFF, MSG);
#define CMD_CMD(CMD, MSG) fmt::print("  {}{:<22}{}  {}.\n", COLOR_YELLOW, CMD, COLOR_OFF, MSG);
#define CMD_CONT(MSG) fmt::print("  {}{:<5} {:<18}{}  {}.\n", COLOR_YELLOW, "", "", COLOR_OFF, MSG);
#define PRINT_LIB(lib, version) fmt::print("  " COLOR_YELLOW " {:<14}" COLOR_OFF " {}\n", lib, version);

#define MATCH(x,y) !strcmp(x,y)
#define STR_CAT(a) #a
#define STR(a) STR_CAT(a)

#define MAX_TARGET_LOUDNESS -5
#define MIN_TARGET_LOUDNESS -30

#define RG_TARGET_LOUDNESS -18.f

typedef enum {
	TYPE_NONE,
	TYPE_STDOUT,
	TYPE_FILE,
} OutputType;

typedef struct {
	char tag_mode;
	double target_loudness;
	double max_peak_level;
	bool true_peak;
	char clip_mode;
	bool do_album;
	OutputType tab_output;
	bool lowercase;
	int id3v2version;
	char opus_mode;
} Config;


void quit(int status);
bool parse_target_loudness(const char *value, double &target_loudness);
bool parse_tag_mode(const char *value, char &mode);
bool parse_clip_mode(const char *value, char &mode);
bool parse_opus_mode(const char *value, char &mode);
bool parse_id3v2_version(const char *value, int &version);
bool parse_max_peak_level(const char *value, double &peak);