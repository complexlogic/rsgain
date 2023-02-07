#pragma once

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

class WorkerThread {

    public:
        WorkerThread(std::unique_ptr<ScanJob> &initial_job, std::mutex &main_mutex, std::mutex &ffmpeg_mutex, std::condition_variable &main_cv, ScanData &data)
        : job(std::move(initial_job)), main_mutex(main_mutex), ffmpeg_mutex(ffmpeg_mutex), main_cv(main_cv), data(data) 
        {
            thread = std::move(std::make_unique<std::thread>(&WorkerThread::work, this));
        }
        void work();
        bool place_job(std::unique_ptr<ScanJob> &job);
        bool wait();
        
    private:
        std::unique_ptr<ScanJob> job;
        std::unique_ptr<std::thread> thread;
        std::mutex &ffmpeg_mutex;
        std::mutex &main_mutex;
        std::condition_variable &main_cv;
        ScanData &data;
        bool quit = false;
        bool job_available = true;
        std::mutex mutex;
        std::condition_variable cv;
};

void easy_mode(int argc, char *argv[]);
void scan_easy(const char *directory, const char *preset, int nb_threads);
const Config& get_config(FileType type);
