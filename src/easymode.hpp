#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <filesystem>
#include <condition_variable>

#ifdef __GNUC__
#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#if GCC_VERSION >= 110000
#define CALC_TIME 1
#else
#define CALC_TIME 0
#endif
#else
#define CALC_TIME 1
#endif

#define MAX_THREAD_SLEEP 30

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

class Job {
    public:
        Job(const std::vector<std::u8string> &file_list, const FileType file_type, const std::filesystem::path &path);
        ~Job();
        int nb_files;
        char **files;
        std::u8string directory;
        Config &config;
};

class WorkerThread {

    public:
        WorkerThread(std::mutex *ffmpeg_mutex, std::mutex *main_mutex, std::condition_variable *main_cv);
        ~WorkerThread();
        void work();
        void add_job(int start_index, int nb_files, char **files);
        bool is_finished(bool &error);
        void quit_thread();
        
    private:
        char **files;
        int nb_files;
        int start_index;
        bool finished;
        bool error;
        bool quit;
        std::mutex *ffmpeg_mutex;
        std::mutex *main_mutex;
        std::mutex thread_mutex;
        std::thread *thread;
        std::condition_variable thread_cv;
        std::condition_variable *main_cv;

};

struct extension_type {
    const char *extension;
    FileType file_type;
};

struct overrides_section {
    const char *section_name;
    FileType file_type;
};

void scan_easy(const char *directory, const char *overrides_file);
static FileType determine_filetype(const char *extension);
static bool is_type(const char *extension, const FileType file_type);
void copy_string_alloc(char **dest, const char *string);