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
#define multithread_progress(name, cur, total) if (!quiet) fmt::print("\33[2K " COLOR_GREEN "{:5.1f}%" COLOR_OFF " Scanning directory '{}'...\r", 100.f * ((float) (cur) / (float) (total)), name); fflush(stdout)

class WorkerThread {

    public:
        WorkerThread(std::mutex *ffmpeg_mutex, std::mutex &main_mutex, std::condition_variable &main_cv, ScanData &scan_data) :
        ffmpeg_mutex(ffmpeg_mutex), main_mutex(main_mutex), main_cv(main_cv), scan_data(scan_data), thread(new std::thread(&WorkerThread::work, this)) {}
        ~WorkerThread() { delete thread; };
        void work();
        bool add_job(ScanJob *job);
        bool wait();
        
    private:
        std::mutex *ffmpeg_mutex;
        std::mutex &main_mutex;
        std::condition_variable &main_cv;
        ScanData &scan_data;
        bool quit = false;
        bool finished = false;
        ScanJob *job = NULL;
        std::mutex thread_mutex;
        std::thread *thread;
        std::condition_variable thread_cv;
};

void easy_mode(int argc, char *argv[]);
void scan_easy(const char *directory, const char *preset, int threads);
