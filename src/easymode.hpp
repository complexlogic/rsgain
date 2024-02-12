#pragma once

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <filesystem>
#include <condition_variable>
#include "scan.hpp"

class WorkerThread {

    public:
        WorkerThread(std::unique_ptr<ScanJob> &initial_job, std::mutex &main_mutex, std::mutex &ffmpeg_mutex, std::condition_variable &main_cv, ScanData &data)
        : job(std::move(initial_job)), main_mutex(main_mutex), ffmpeg_mutex(ffmpeg_mutex), main_cv(main_cv), data(data) 
        {
            thread = std::make_unique<std::thread>(&WorkerThread::work, this);
        }
        void work();
        bool place_job(std::unique_ptr<ScanJob> &job);
        bool wait();
        
    private:
        std::unique_ptr<ScanJob> job;
        std::mutex &main_mutex;
        std::mutex &ffmpeg_mutex;
        std::condition_variable &main_cv;
        ScanData &data;
        std::unique_ptr<std::thread> thread;
        bool quit = false;
        bool job_available = true;
        std::mutex mutex;
        std::condition_variable cv;
};

void easy_mode(int argc, char *argv[]);
void scan_easy(const std::filesystem::path &path, const std::filesystem::path &preset, size_t nb_threads);
const Config& get_config(FileType type);
