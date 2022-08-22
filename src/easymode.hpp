#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <filesystem>
#include <condition_variable>
#include "scan.hpp"

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
#define MAX_THREADS -1

typedef struct ScanData {
    unsigned long files;
    unsigned long clippings_prevented;
    std::vector<std::string> error_directories;

    ScanData(void) : files(0), clippings_prevented(0){};
    void update(ScanJob *job);
} ScanData;

class WorkerThread {

    public:
        WorkerThread(std::mutex *ffmpeg_mutex, std::mutex &main_mutex, std::condition_variable &main_cv, ScanData &scan_data);
        ~WorkerThread();
        void work();
        bool add_job(ScanJob *job);
        bool wait();
        
    private:
        std::mutex *ffmpeg_mutex;
        std::mutex &main_mutex;
        std::condition_variable &main_cv;
        ScanData &scan_data;
        bool quit;
        bool finished;
        ScanJob *job;
        std::mutex thread_mutex;
        std::thread *thread;
        std::condition_variable thread_cv;

};


struct overrides_section {
    const char *name;
    FileType file_type;
};

void easy_mode(int argc, char *argv[]);
void scan_easy(const char *directory, const char *overrides_file, int threads);
