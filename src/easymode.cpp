#include <filesystem>
#include <vector>
#include <deque>
#include <chrono>
#include <set>
#include <string>
#include <thread>
#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <iomanip>
#include <locale>
#include <string.h>
#include <stdio.h>
#include <ini.h>
#include <fmt/core.h>
#include "rsgain.hpp"
#include "easymode.hpp"
#include "output.hpp"
#include "scan.hpp"

extern int multithread;

// Default configs
static Config configs[] = {
    
    // MP2 config
    {
        .mode = 'i',
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = EBU_R128_MAX_PEAK,
        .true_peak = true,
        .no_clip = true,
        .do_album = true,
        .tab_output = false,
        .lowercase = true,
        .strip = true,
        .id3v2version = 3
    },

    // MP3 config
    {
        .mode = 'i',
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = EBU_R128_MAX_PEAK,
        .true_peak = true,
        .no_clip = true,
        .do_album = true,
        .tab_output = false,
        .lowercase = true,
        .strip = true,
        .id3v2version = 3
    },

    // FLAC config
    {
        .mode = 'i',
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = EBU_R128_MAX_PEAK,
        .true_peak = true,
        .no_clip = true,
        .do_album = true,
        .tab_output = false,
        .lowercase = false,
        .strip = false,
        .id3v2version = 4
    },

    // OGG config
    {
        .mode = 'i',
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = EBU_R128_MAX_PEAK,
        .true_peak = true,
        .no_clip = true,
        .do_album = true,
        .tab_output = false,
        .lowercase = false,
        .strip = false,
        .id3v2version = 4
    },

    // OPUS config
    {
        .mode = 'i',
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = EBU_R128_MAX_PEAK,
        .true_peak = true,
        .no_clip = true,
        .do_album = true,
        .tab_output = false,
        .lowercase = false,
        .strip = false,
        .id3v2version = 4
    },

    // M4A config
    {
        .mode = 'i',
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = EBU_R128_MAX_PEAK,
        .true_peak = true,
        .no_clip = true,
        .do_album = true,
        .tab_output = false,
        .lowercase = true,
        .strip = false,
        .id3v2version = 4
    },

    // WMA config
    {
        .mode = 'i',
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = EBU_R128_MAX_PEAK,
        .true_peak = true,
        .no_clip = true,
        .do_album = true,
        .tab_output = false,
        .lowercase = true,
        .strip = false,
        .id3v2version = 4
    },

    // WAV config
    {
        .mode = 'i',
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = EBU_R128_MAX_PEAK,
        .true_peak = true,
        .no_clip = true,
        .do_album = true,
        .tab_output = false,
        .lowercase = true,
        .strip = false,
        .id3v2version = 3
    },

    // AIFF config
    {
        .mode = 'i',
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = EBU_R128_MAX_PEAK,
        .true_peak = true,
        .no_clip = true,
        .do_album = true,
        .tab_output = false,
        .lowercase = true,
        .strip = false,
        .id3v2version = 3
    },

    // WAVPACK config
    {
        .mode = 'i',
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = EBU_R128_MAX_PEAK,
        .true_peak = true,
        .no_clip = true,
        .do_album = true,
        .tab_output = false,
        .lowercase = false,
        .strip = true,
        .id3v2version = 4
    },

    // APE config
    {
        .mode = 'i',
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = EBU_R128_MAX_PEAK,
        .true_peak = true,
        .no_clip = true,
        .do_album = true,
        .tab_output = false,
        .lowercase = false,
        .strip = true,
        .id3v2version = 4
    },
};


static void convert_bool(const char *value, bool &setting)
{
    if(MATCH(value, "True") || MATCH(value, "true")) {
        setting = true;
    }
    else if (MATCH(value, "False") || MATCH(value, "false")) {
        setting = false;
    }
}

static inline FileType determine_section_type(const std::string &section)
{
    static const struct overrides_section sections[] {
        {"MP2",     MP2},
        {"MP3",     MP3},
        {"FLAC",    FLAC},
        {"OGG",     OGG},
        {"OPUS",    OPUS},
        {"M4A",     M4A},
        {"WMA",     WMA},
        {"WAV",     WAV},
        {"AIFF",    AIFF},
        {"WAVPACK", WAVPACK},
        {"APE",     APE}
    };

    auto it = std::find_if(std::cbegin(sections), std::cend(sections), [&](auto s){return s.name == section;});
    return it == std::cend(sections) ? INVALID : it->file_type;
}

// Callback for INI parser
static int handler(void *user, const char *section, const char *name, const char *value)
{
    FileType file_type = determine_section_type(section);
    if (file_type == INVALID) {
        return 0;
    }

    // Parse setting keys
    if (MATCH(name, "Album")) {
        convert_bool(value, configs[file_type].do_album);
    }
    else if (MATCH(name, "Mode")) {
        parse_mode(value, configs[file_type]);
    }
    else if (MATCH(name, "ClippingProtection")) {
        convert_bool(value, configs[file_type].no_clip);
    }
    else if (MATCH(name, "Lowercase")) {
        convert_bool(value, configs[file_type].lowercase);
    }
    else if (MATCH(name, "Strip")) {
        convert_bool(value, configs[file_type].strip);
    }
    else if (MATCH(name, "ID3v2Version")) {
        parse_id3v2_version(value, configs[file_type]);
    }
    else if (MATCH(name, "TargetLoudness")) {
        parse_target_loudness(value, configs[file_type]);
    }
    else if (MATCH(name, "MaxPeakLevel")) {
        parse_max_peak_level(value, configs[file_type]);
    }
    else if (MATCH(name, "TruePeak")) {
        convert_bool(value, configs[file_type].true_peak);
    }
    return 0;
}

// Override the default easy mode settings
static void load_overrides(const char *overrides_file)
{
    std::filesystem::path path((char8_t*) overrides_file);
    if (!std::filesystem::exists(path)) {
        fmt::print("Error: Overrides file '{}' does not exist\n", overrides_file);
        return;
    }
    if (!std::filesystem::is_regular_file(path)) {
        fmt::print("Error: Overrides file '{}' is not valid\n", overrides_file);
        return;
    }

    // Parse file
    output_ok("Applying overrides");
    if (ini_parse(overrides_file, handler, NULL) < 0) {
        fmt::print("Failed to load overrides file '{}'\n", overrides_file);
    }
}

void ScanData::update(ScanJob *job)
{
    if (job->error) {
        error_directories.push_back(job->path);
        return;
    }
    files += job->nb_files;
    clippings_prevented += job->clippings_prevented;
}

WorkerThread::WorkerThread(std::mutex *ffmpeg_mutex, std::mutex &main_mutex, std::condition_variable &main_cv, ScanData &scan_data) :
ffmpeg_mutex(ffmpeg_mutex), main_mutex(main_mutex), main_cv(main_cv), scan_data(scan_data), quit(false), finished(false), job(NULL)
{
    thread = new std::thread(&WorkerThread::work, this);
}

bool WorkerThread::add_job(ScanJob *job)
{
    std::unique_lock lk(thread_mutex, std::try_to_lock);
    if (!lk.owns_lock() || this->job != NULL)
        return false;
    
    this->job = job;
    lk.unlock();
    thread_cv.notify_all();
    return true;
}

WorkerThread::~WorkerThread()
{
    delete thread;
}

void WorkerThread::work()
{
    std::unique_lock main_lock(main_mutex, std::defer_lock);
    std::unique_lock thread_lock(thread_mutex);
    while (!quit) {
        if (job != NULL) {
            job->scan(configs[job->type], ffmpeg_mutex);

            // Inform the main thread that the scanning is finished
            main_lock.lock();
            scan_data.update(job);
            delete job;
            job = NULL;
            main_lock.unlock();
            main_cv.notify_all();
        }
                                                                                                                                                                                                                                                                                                                                                                        
        // Wait until we get a new job from the main thread
        thread_cv.wait_for(thread_lock, std::chrono::seconds(MAX_THREAD_SLEEP));
    }
    return;
}


bool WorkerThread::wait()
{
    std::unique_lock lk(thread_mutex, std::try_to_lock);
    if (!lk.owns_lock() || job != NULL)
        return false;

    quit = true;
    lk.unlock();
    thread_cv.notify_all();
    thread->join();
    return true;
}


void scan_easy(const char *directory, const char *overrides_file)
{
    std::filesystem::path path(directory);
    ScanData scan_data;

    // Verify directory exists and is valid
    if (!std::filesystem::exists(path)) {
        fmt::print("Error: Directory '{}' does not exist\n", directory);
        quit(EXIT_FAILURE);
    }
    else if (!std::filesystem::is_directory(path)) {
        fmt::print("Error: '{}' is not a valid directory\n", directory);
        quit(EXIT_FAILURE);
    }

    // Load overrides
    if (overrides_file != NULL) {
        load_overrides(overrides_file);
    }

    // Make sure the requested threads is 1 per CPU core or fewer
    if (multithread) {
        int cores = std::thread::hardware_concurrency();
        if (multithread > cores) {
            multithread = cores;
            if (multithread <= 1) {
                multithread = 0;
            }
        }
    }

    // Record start time
    const auto start_time = std::chrono::system_clock::now();

    // Generate queue of all directories in directory tree
    output_ok("Building directory tree...");
    std::deque<std::filesystem::path> directories;
    directories.push_back(path);
    for (const std::filesystem::directory_entry &entry : std::filesystem::recursive_directory_iterator(path)) {
        if (entry.is_directory()) {
            directories.push_back(entry.path());
        }
    }

    // Multithread scannning
    if (multithread) {
        std::vector<WorkerThread*> worker_threads;
        std::mutex ffmpeg_mutex;
        std::mutex main_mutex;
        std::unique_lock main_lock(main_mutex);
        std::condition_variable main_cv;

        // Create threads
        for (int i = 0; i < multithread; i++) {
            worker_threads.push_back(new WorkerThread(&ffmpeg_mutex, main_mutex, main_cv, scan_data));
        }

        ScanJob *job;
        while (directories.size()) {
            job = new ScanJob();
            std::filesystem::path &dir = directories.front();
            if (job->add_directory(dir) != INVALID) {
                bool job_placed = false;
                while (!job_placed) {
                    for (auto wt = worker_threads.begin(); wt != worker_threads.end() && !job_placed; ++wt) {
                        job_placed = (*wt)->add_job(job);
                    }
                    if (job_placed) {
                        output_ok("Scanning directory '{}'", dir.string());
                    }

                    // Wait until one of the worker threads informs that us that it is ready for a new job
                    else {
                        main_cv.wait_for(main_lock, std::chrono::seconds(MAX_THREAD_SLEEP));
                    }
                }
            }
            else {
                delete job;
            }
            directories.pop_front();
        }

        // Wait for threads to finish scanning
        while (worker_threads.size()) {
            for (auto wt = worker_threads.begin(); wt != worker_threads.end();) {
                (*wt)->wait() ? worker_threads.erase(wt) : ++wt;
            }

            if (worker_threads.size())
                main_cv.wait_for(main_lock, std::chrono::seconds(MAX_THREAD_SLEEP));
        }
    }
    
    // Single threaded scanning
    else {
        FileType file_type;
        while (directories.size()) {
            ScanJob job;
            if ((file_type = job.add_directory(directories.front())) != INVALID) {
                output_ok("Scanning directory: '{}'", job.path);
                job.scan(configs[file_type]);
                scan_data.update(&job);
            }
            directories.pop_front();
        }
    }

    const auto end_time = std::chrono::system_clock::now();
    if (!scan_data.files) {
        fmt::print("No files were scanned\n");
        return;
    }
    fmt::print(COLOR_GREEN "Scanning Complete" COLOR_OFF "\n");

    // Calculate time (not available in GCC 10 and earlier)
#if CALC_TIME
    std::string time_string;
    std::chrono::hh_mm_ss elapsed(end_time - start_time);
    int h = elapsed.hours().count();
    int m = elapsed.minutes().count();
    int s = elapsed.seconds().count();
    if (h) {
        time_string = std::to_string(h) + "h " + std::to_string(m) + "m";
    }
    else if (m) {
        time_string = std::to_string(m) + "m " + std::to_string(s) + "s";
    }
    else {
        time_string = std::to_string(s) + "s";
    }
#endif

    // Format file and clip strings
    std::stringstream file_ss;
    file_ss.imbue(std::locale(""));
    file_ss << std::fixed << scan_data.files;

    std::stringstream clip_ss;
    
    fmt::print(COLOR_YELLOW "Files Scanned:" COLOR_OFF " {}\n", file_ss.str());

#if CALC_TIME
    fmt::print(COLOR_YELLOW "Time Elapsed:" COLOR_OFF "  {}\n", time_string);
#endif
    if (scan_data.clippings_prevented) {
        clip_ss.imbue(std::locale(""));
        clip_ss << std::fixed << scan_data.clippings_prevented;
        fmt::print(COLOR_YELLOW "Clippings Prevented: " COLOR_OFF "{} ({:.1f}% of files)\n", clip_ss.str(), 100.f * (float) scan_data.clippings_prevented / (float) scan_data.files);
    }
    fmt::print("\n");

    // Inform user of errors
    if (scan_data.error_directories.size()) {
        fmt::print(COLOR_RED "There were errors while scanning the following directories:" COLOR_OFF "\n");
        for (std::string &s : scan_data.error_directories) {
            fmt::print("{}\n", s);
        }
        fmt::print("\n");
    }
}