#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <filesystem>
#include <condition_variable>
#include <fmt/core.h>
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

#define HELP_STATS(title, format, ...) fmt::print(COLOR_YELLOW "{:<18} " COLOR_OFF format "\n", title ":" __VA_OPT__(,) __VA_ARGS__)
#define multithread_progress(name, cur, total) if (!quiet) fmt::print("\33[2K " COLOR_GREEN "{:5.1f}%" COLOR_OFF " Scanning directory '{}'...\r", 100.f * ((float) (cur) / (float) (total)), name); fflush(stdout)
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
