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
#include <initializer_list>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <ini.h>
#include <getopt.h>
#include <fmt/core.h>
#include <config.h>
#include "rsgain.hpp"
#include "easymode.hpp"
#include "output.hpp"
#include "scan.hpp"

extern "C" {
    int format_handler(void *user, const char *section, const char *name, const char *value);
    int global_handler(void *user, const char *section, const char *name, const char *value);
}

static inline void help_easy(void);
bool multithread = false;

#ifdef DEBUG
    int infinite_loop = 0;
#endif

// Default configs
static Config configs[] = {
    
    // MP2 config
    {
        .tag_mode = 'i',
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.f,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = TYPE_NONE,
        .lowercase = false,
        .id3v2version = 3,
        .opus_mode = 'd'
    },

    // MP3 config
    {
        .tag_mode = 'i',
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.f,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = TYPE_NONE,
        .lowercase = false,
        .id3v2version = 3,
        .opus_mode = 'd'
    },

    // FLAC config
    {
        .tag_mode = 'i',
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.f,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = TYPE_NONE,
        .lowercase = false,
        .id3v2version = 3,
        .opus_mode = 'd'
    },

    // OGG config
    {
        .tag_mode = 'i',
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.f,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = TYPE_NONE,
        .lowercase = false,
        .id3v2version = 3,
        .opus_mode = 'd'
    },

    // OPUS config
    {
        .tag_mode = 'i',
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.f,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = TYPE_NONE,
        .lowercase = false,
        .id3v2version = 3,
        .opus_mode = 'd'
    },

    // M4A config
    {
        .tag_mode = 'i',
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.f,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = TYPE_NONE,
        .lowercase = false,
        .id3v2version = 3,
        .opus_mode = 'd'
    },

    // WMA config
    {
        .tag_mode = 'i',
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.f,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = TYPE_NONE,
        .lowercase = false,
        .id3v2version = 3,
        .opus_mode = 'd'
    },

    // WAV config
    {
        .tag_mode = 'i',
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.f,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = TYPE_NONE,
        .lowercase = false,
        .id3v2version = 3,
        .opus_mode = 'd'
    },

    // AIFF config
    {
        .tag_mode = 'i',
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.f,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = TYPE_NONE,
        .lowercase = false,
        .id3v2version = 3,
        .opus_mode = 'd'
    },

    // Wavpack config
    {
        .tag_mode = 'i',
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.f,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = TYPE_NONE,
        .lowercase = false,
        .id3v2version = 3,
        .opus_mode = 'd'
    },

    // APE config
    {
        .tag_mode = 'i',
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.f,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = TYPE_NONE,
        .lowercase = false,
        .id3v2version = 3,
        .opus_mode = 'd'
    },
};

// Parse Easy Mode command line arguments
void easy_mode(int argc, char *argv[])
{
    int rc, i;
    char *preset = NULL;
    const char *short_opts = "+hql:m:p:O";
    int threads = 1;

    static struct option long_opts[] = {
        { "help",          no_argument,       NULL, 'h' },
        { "quiet",         no_argument,       NULL, 'q' },

        { "multithread",   required_argument, NULL, 'm' },
        { "preset",        required_argument, NULL, 'p' },
        { "output",        required_argument, NULL, 'O' },
#ifdef DEBUG
        { "infinite-loop", no_argument,       &infinite_loop, 1 },
#endif
        { 0, 0, 0, 0 }
    };
    while ((rc = getopt_long(argc, argv, short_opts, long_opts, &i)) != -1) {
        switch (rc) {
            case 'h':
                help_easy();
                quit(EXIT_SUCCESS);
                break;

            case 'q':
                quiet = true;
                break;
            
            case 'm':
                {
                    int max_threads = std::thread::hardware_concurrency();
                    if (!max_threads)
                        max_threads = 1;
                    if (MATCH(optarg, "MAX") || MATCH(optarg, "max")) {
                        threads = max_threads;
                    }
                    else {
                        threads = atoi(optarg);
                        if (threads < 1) {
                            threads = 1;
                        }
                        else if (threads > max_threads) {
                            output_warn("{} threads were requested, but only {} are available", threads, max_threads);
                            threads = max_threads;
                        }
                    }
                    multithread = (threads > 1);
                }
                break;
            
            case 'p':
                if (preset == NULL)
                    preset = optarg;
                break;

            case 'O':
                for (Config &config : configs)
                    config.tab_output = TYPE_FILE;
                break;
        }
    }

    if (argc == optind) {
        output_fail("No directory specified");
        quit(EXIT_FAILURE);
    }

    scan_easy(argv[optind], preset, threads);
}

static bool convert_bool(const char *value, bool &setting)
{
    if(MATCH(value, "True") || MATCH(value, "true")) {
        setting = true;
        return true;
    }
    if (MATCH(value, "False") || MATCH(value, "false")) {
        setting = false;
        return true;
    }
    return false;
}

static inline FileType determine_section_type(const std::string &section)
{
    static const struct preset_section sections[] {
        {"MP2",     MP2},
        {"MP3",     MP3},
        {"FLAC",    FLAC},
        {"Ogg",     OGG},
        {"Opus",    OPUS},
        {"M4A",     M4A},
        {"WMA",     WMA},
        {"WAV",     WAV},
        {"AIFF",    AIFF},
        {"Wavpack", WAVPACK},
        {"APE",     APE}
    };

    auto it = std::find_if(std::cbegin(sections), std::cend(sections), [&](auto s){return s.name == section;});
    return it == std::cend(sections) ? INVALID : it->file_type;
}

// Callback for INI parser
int global_handler(void *user, const char *section, const char *name, const char *value)
{
    if (strcmp(section, "Global"))
        return 0;

    // Parse setting keys
    if (MATCH(name, "Album")) {
        bool do_album;
        if(convert_bool(value, do_album)) {
            for (Config &config : configs)
                config.do_album = do_album;
        }
    }
    else if (MATCH(name, "TagMode")) {
        char tag_mode;
        if (parse_tag_mode(value, tag_mode)) {
            for (Config &config : configs)
                config.tag_mode = tag_mode;
        }
    }
    else if (MATCH(name, "ClipMode")) {
        char clip_mode;
        if (parse_clip_mode(value, clip_mode)) {
            for (Config &config : configs)
                config.clip_mode = clip_mode;
        }
    }
    else if (MATCH(name, "TargetLoudness")) {
        double target_loudness;
        if (parse_target_loudness(value, target_loudness)) {
            for (Config &config : configs)
                config.target_loudness = target_loudness;
        }
    }
    else if (MATCH(name, "MaxPeakLevel")) {
        double max_peak_level;
        if (parse_max_peak_level(value, max_peak_level)) {
            for (Config &config : configs)
                config.max_peak_level = max_peak_level;
        }
    }
    else if (MATCH(name, "TruePeak")) {
        bool true_peak;
        if (convert_bool(value, true_peak)) {
            for (Config &config : configs)
                config.true_peak = true_peak;
        }
    }
    return 0;
}

int format_handler(void *user, const char *section, const char *name, const char *value)
{
    FileType file_type = determine_section_type(section);
    if (file_type == INVALID)
        return 0;

    // Parse setting keys
    if (MATCH(name, "Album")) {
        convert_bool(value, configs[file_type].do_album);
    }
    else if (MATCH(name, "TagMode")) {
        parse_tag_mode(value, configs[file_type].tag_mode);
    }
    else if (MATCH(name, "ClipMode")) {
        parse_clip_mode(value, configs[file_type].clip_mode);
    }
    else if (MATCH(name, "Lowercase")) {
        convert_bool(value, configs[file_type].lowercase);
    }
    else if (MATCH(name, "ID3v2Version")) {
        parse_id3v2_version(value, configs[file_type].id3v2version);
    }
    else if (MATCH(name, "TargetLoudness")) {
        parse_target_loudness(value, configs[file_type].target_loudness);
    }
    else if (MATCH(name, "MaxPeakLevel")) {
        parse_max_peak_level(value, configs[file_type].max_peak_level);
    }
    else if (MATCH(name, "TruePeak")) {
        convert_bool(value, configs[file_type].true_peak);
    }
    else if (MATCH(name, "OpusMode")) {
        parse_opus_mode(value, configs[file_type].opus_mode);
    }
    return 0;
}

inline void join_paths(std::filesystem::path &p, std::initializer_list<const char*> list)
{
    for (const char *item : list) { // Appending null string will cause segfault
        if (item == NULL)
            return;
    }
    
    auto it = list.begin();
    p = *it;
    it++;
    for (it; it != list.end(); ++it)
        p /= *it;
}

static void load_preset(const char *preset)
{
    std::filesystem::path path(preset);

    // Find preset file from name
    if (!path.has_extension()) {

        // Mac/Linux check user directory before system directory
#ifdef __unix__
#ifdef __APPLE__
        join_paths(path, {(const char*) getenv("HOME"), "Library", EXECUTABLE_TITLE, "presets", preset});
#endif
#ifdef __linux
        join_paths(path, {(const char*) getenv("HOME"), ".config", EXECUTABLE_TITLE, "presets", preset});
#endif
        path += ".ini";

        // Check system directory
        if (!std::filesystem::exists(path)) {
            join_paths(path, {PRESETS_DIR, preset});
            path += ".ini";
        }
#endif

        // Only one preset folder on Windows
#ifdef _WIN32
        char buffer[MAX_PATH];
        if (GetModuleFileNameA(NULL, buffer, sizeof(buffer))) {
            std::filesystem::path exe = buffer;
            join_paths(path, {exe.parent_path().string().c_str(), "presets", preset});
            path += ".ini";
        }
#endif
    }

    if (!std::filesystem::exists(path)) {
        output_error("Could not locate preset '{}'", preset);
        return;
    }

    // Parse file
    std::FILE *file = fopen(path.string().c_str(), "r");
    if (file == NULL) {
        output_error("Failed to open preset from '{}'", path.string());
        return;
    }

    output_ok("Applying preset...");
    ini_parse_file(file, global_handler, NULL);
    rewind(file);
    ini_parse_file(file, format_handler, NULL);
    fclose(file);
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
            job->update_data(scan_data);
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

void scan_easy(const char *directory, const char *preset, int threads)
{
    std::filesystem::path path(directory);
    ScanData scan_data;

    // Verify directory exists and is valid
    if (!std::filesystem::exists(path)) {
        output_fail("Directory '{}' does not exist", directory);
        quit(EXIT_FAILURE);
    }
    else if (!std::filesystem::is_directory(path)) {
        output_fail("'{}' is not a valid directory", directory);
        quit(EXIT_FAILURE);
    }

    // Load scan preset
    if (preset != NULL)
        load_preset(preset);

    // Record start time
    const auto start_time = std::chrono::system_clock::now();

    // Generate queue of all directories in directory tree
    output_ok("Building directory tree...");
    std::deque<std::filesystem::path> directories;
    directories.push_back(path);
    for (const std::filesystem::directory_entry &entry : std::filesystem::recursive_directory_iterator(path)) {
        if (entry.is_directory())
            directories.push_back(entry.path());
    }
    int num_directories = directories.size();
    output_ok("Found {:L} {}...", num_directories, num_directories > 1 ? "directories" : "directory");

#ifdef DEBUG
    std::deque<std::filesystem::path> directories_static;
    if (infinite_loop)
        directories_static = directories;
#endif

    // Multithread scannning
    if (multithread) {
        std::vector<WorkerThread*> worker_threads;
        std::mutex ffmpeg_mutex;
        std::mutex main_mutex;
        std::unique_lock main_lock(main_mutex);
        std::condition_variable main_cv;

        // Create threads
        for (int i = 0; i < threads; i++)
            worker_threads.push_back(new WorkerThread(&ffmpeg_mutex, main_mutex, main_cv, scan_data));

        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Wait a bit for the threads to initialize;

        ScanJob *job;
        output_ok("Scanning with {} threads...", threads);
        while (directories.size()) {
            job = new ScanJob();
            std::filesystem::path &dir = directories.front();
            if (job->add_directory(dir) != INVALID) {
                bool job_placed = false;

                // Feed the generated job to the first available worker thread
                while (!job_placed) {
                    for (auto wt = worker_threads.begin(); wt != worker_threads.end() && !job_placed; ++wt) {
                        job_placed = (*wt)->add_job(job);
                    }
                    if (job_placed) {
                        multithread_progress(dir.string(), num_directories - directories.size(), num_directories);
                    }

                    // Wait until one of the worker threads informs that us that it is ready for a new job
                    else {
                        main_cv.wait_for(main_lock, std::chrono::seconds(MAX_THREAD_SLEEP));
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                }
            }
            else {
                delete job;
            }
            directories.pop_front();

#ifdef DEBUG
            if (infinite_loop && !directories.size())
                directories = directories_static;
#endif
        }

        // Wait for worker threads to finish scanning
        while (worker_threads.size()) {
            for (auto wt = worker_threads.begin(); wt != worker_threads.end();)
                if((*wt)->wait()) {
                    delete *wt;
                    worker_threads.erase(wt);
                }
                else {
                    ++wt;
                }

            if (worker_threads.size()) {
                main_cv.wait_for(main_lock, std::chrono::seconds(1));
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Sometimes we have to wait a bit for the thread to release the mutex
            }
        }
        fmt::print("\33[2K\n");
    }
    
    // Single threaded scanning
    else {
        FileType file_type;
        while (directories.size()) {
            ScanJob job;
            if ((file_type = job.add_directory(directories.front())) != INVALID) {
                output_ok("Scanning directory: '{}'", job.path);
                job.scan(configs[file_type]);
                job.update_data(scan_data);
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

    // Format time string
    int s = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
    int h = s / 3600;
    s -= h * 3600;
    int m = s / 60;
    s -= m * 60;
    std::string time_string;
    if (h) {
        time_string = fmt::format("{}h {}m", h, m);
    }
    else if (m) {
        time_string = fmt::format("{}m {}s", m, s);
    }
    else {
        time_string = fmt::format("{}s", s);
    }

    HELP_STATS("Time Elapsed", "{}", time_string);
    HELP_STATS("Files Scanned", "{:L}", scan_data.files);
    HELP_STATS("Clip Adjustments", "{:L} ({:.1f}% of files)", scan_data.clipping_adjustments, 100.f * (float) scan_data.clipping_adjustments / (float) scan_data.files);
    HELP_STATS("Average Gain", "{:.2f} dB", scan_data.total_gain / (double) scan_data.files);
    double average_peak = scan_data.total_peak / (double) scan_data.files;
    HELP_STATS("Average Peak", "{:.6f} ({:.2f} dB)", average_peak, 20.0 * log10(average_peak));
    HELP_STATS("Negative Gains", "{:L} ({:.1f}% of files)", scan_data.total_negative, 100.f * (float) scan_data.total_negative / (float) scan_data.files);
    HELP_STATS("Positive Gains", "{:L} ({:.1f}% of files)", scan_data.total_positive, 100.f * (float) scan_data.total_positive / (float) scan_data.files);
    fmt::print("\n");

    // Inform user of errors
    if (scan_data.error_directories.size()) {
        fmt::print(COLOR_RED "There were errors while scanning the following directories:" COLOR_OFF "\n");
        for (std::string &s : scan_data.error_directories)
            fmt::print("{}\n", s);
        fmt::print("\n");
    }
}

static inline void help_easy(void) {
    fmt::print(COLOR_RED "Usage: " COLOR_OFF "{}{}{} easy [OPTIONS] DIRECTORY\n", COLOR_GREEN, EXECUTABLE_TITLE, COLOR_OFF);

    fmt::print("  Easy Mode recursively scans a directory using the recommended settings for each\n");
    fmt::print("  file type. Easy Mode assumes that you have your music library organized with each album\n");
    fmt::print("  in its own folder.\n");

    fmt::print("\n");
    fmt::print(COLOR_RED "Options:\n" COLOR_OFF);

    CMD_HELP("--help",     "-h", "Show this help");
    CMD_HELP("--quiet",      "-q",  "Don't print scanning status messages");
    fmt::print("\n");

    CMD_HELP("--multithread=n", "-m n", "Scan files with n parallel threads");
    CMD_HELP("--preset=s", "-p s", "Load scan preset s");
    CMD_HELP("--output", "-O",  "Output tab-delimited scan data to CSV file per directory");

    fmt::print("\n");

    fmt::print("Please report any issues to " PROJECT_URL "/issues\n");
    fmt::print("\n");
}