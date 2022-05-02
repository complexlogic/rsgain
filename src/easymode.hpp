#include "easymode.h"
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

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
        Config *config;
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

static FileType determine_type(const char *extension);
static bool is_type(const char *extension, const FileType file_type);
void copy_string_alloc(char **dest, const char *string);