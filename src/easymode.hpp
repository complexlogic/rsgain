#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <filesystem>
#include <condition_variable>
#include <fmt/core.h>
#include "scan.hpp"

#define MAX_THREAD_SLEEP 30

#define HELP_STATS(title, format, ...) fmt::print(COLOR_YELLOW "{:<18} " COLOR_OFF format "\n", title ":" __VA_OPT__(,) __VA_ARGS__)
#define multithread_progress(name, cur, total) if (!quiet) fmt::print("\33[2K " COLOR_GREEN "{:5.1f}%" COLOR_OFF " Scanning directory '{}'\r", 100.f * ((float) (cur) / (float) (total)), name); fflush(stdout)

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

struct preset_section {
    const char *name;
    FileType file_type;
};

void easy_mode(int argc, char *argv[]);
void scan_easy(const char *directory, const char *preset_file, int threads);
