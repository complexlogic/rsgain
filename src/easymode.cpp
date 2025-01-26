#include <filesystem>
#include <vector>
#include <queue>
#include <chrono>
#include <set>
#include <string>
#include <thread>
#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <initializer_list>
#include <unordered_map>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <ini.h>
#include <getopt.h>

#include <config.h>
#include "rsgain.hpp"
#include "easymode.hpp"
#include "output.hpp"
#include "scan.hpp"

#define MAX_THREAD_SLEEP 30
#define HELP_STATS(title, format, ...) rsgain::print(COLOR_YELLOW "{:<18} " COLOR_OFF format "\n", title ":" __VA_OPT__(,) __VA_ARGS__)

extern "C" {
    int format_handler(void *user, const char *section, const char *name, const char *value);
    int global_handler(void *user, const char *section, const char *name, const char *value);
}

static inline void help_easy();
bool multithread = false;

static Config configs[] = {

    // Default config
    {
        .tag_mode = 'i',
        .skip_existing = false,
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.0,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = OutputType::NONE,
        .sep_header = false,
        .sort_alphanum = false,
        .lowercase = false,
        .id3v2version = ID3V2_KEEP,
        .opus_mode = 'd',
        .skip_mp4 = false,
        .preserve_mtimes = false,
        .dual_mono = false
    },

    // MP2 config
    {
        .tag_mode = 'i',
        .skip_existing = false,
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.0,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = OutputType::NONE,
        .sep_header = false,
        .sort_alphanum = false,
        .lowercase = false,
        .id3v2version = ID3V2_KEEP,
        .opus_mode = 'd',
        .skip_mp4 = false,
        .preserve_mtimes = false,
        .dual_mono = false
    },

    // MP3 config
    {
        .tag_mode = 'i',
        .skip_existing = false,
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.0,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = OutputType::NONE,
        .sep_header = false,
        .sort_alphanum = false,
        .lowercase = false,
        .id3v2version = ID3V2_KEEP,
        .opus_mode = 'd',
        .skip_mp4 = false,
        .preserve_mtimes = false,
        .dual_mono = false
    },

    // FLAC config
    {
        .tag_mode = 'i',
        .skip_existing = false,
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.0,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = OutputType::NONE,
        .sep_header = false,
        .sort_alphanum = false,
        .lowercase = false,
        .id3v2version = ID3V2_KEEP,
        .opus_mode = 'd',
        .skip_mp4 = false,
        .preserve_mtimes = false,
        .dual_mono = false
    },

    // OGG config
    {
        .tag_mode = 'i',
        .skip_existing = false,
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.0,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = OutputType::NONE,
        .sep_header = false,
        .sort_alphanum = false,
        .lowercase = false,
        .id3v2version = ID3V2_KEEP,
        .opus_mode = 'd',
        .skip_mp4 = false,
        .preserve_mtimes = false,
        .dual_mono = false
    },

    // OPUS config
    {
        .tag_mode = 'i',
        .skip_existing = false,
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.0,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = OutputType::NONE,
        .sep_header = false,
        .sort_alphanum = false,
        .lowercase = false,
        .id3v2version = ID3V2_KEEP,
        .opus_mode = 'd',
        .skip_mp4 = false,
        .preserve_mtimes = false,
        .dual_mono = false
    },

    // M4A config
    {
        .tag_mode = 'i',
        .skip_existing = false,
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.0,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = OutputType::NONE,
        .sep_header = false,
        .sort_alphanum = false,
        .lowercase = false,
        .id3v2version = ID3V2_KEEP,
        .opus_mode = 'd',
        .skip_mp4 = false,
        .preserve_mtimes = false,
        .dual_mono = false
    },

    // WMA config
    {
        .tag_mode = 'i',
        .skip_existing = false,
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.0,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = OutputType::NONE,
        .sep_header = false,
        .sort_alphanum = false,
        .lowercase = false,
        .id3v2version = ID3V2_KEEP,
        .opus_mode = 'd',
        .skip_mp4 = false,
        .preserve_mtimes = false,
        .dual_mono = false
    },

    // WAV config
    {
        .tag_mode = 'i',
        .skip_existing = false,
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.0,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = OutputType::NONE,
        .sep_header = false,
        .sort_alphanum = false,
        .lowercase = false,
        .id3v2version = ID3V2_KEEP,
        .opus_mode = 'd',
        .skip_mp4 = false,
        .preserve_mtimes = false,
        .dual_mono = false
    },

    // AIFF config
    {
        .tag_mode = 'i',
        .skip_existing = false,
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.0,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = OutputType::NONE,
        .sep_header = false,
        .sort_alphanum = false,
        .lowercase = false,
        .id3v2version = ID3V2_KEEP,
        .opus_mode = 'd',
        .skip_mp4 = false,
        .preserve_mtimes = false,
        .dual_mono = false
    },

    // Wavpack config
    {
        .tag_mode = 'i',
        .skip_existing = false,
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.0,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = OutputType::NONE,
        .sep_header = false,
        .sort_alphanum = false,
        .lowercase = false,
        .id3v2version = ID3V2_KEEP,
        .opus_mode = 'd',
        .skip_mp4 = false,
        .preserve_mtimes = false,
        .dual_mono = false
    },

    // APE config
    {
        .tag_mode = 'i',
        .skip_existing = false,
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.0,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = OutputType::NONE,
        .sep_header = false,
        .sort_alphanum = false,
        .lowercase = false,
        .id3v2version = ID3V2_KEEP,
        .opus_mode = 'd',
        .skip_mp4 = false,
        .preserve_mtimes = false,
        .dual_mono = false
    },

    // TAK config
    {
        .tag_mode = 'i',
        .skip_existing = false,
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.0,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = OutputType::NONE,
        .sep_header = false,
        .sort_alphanum = false,
        .lowercase = false,
        .id3v2version = ID3V2_KEEP,
        .opus_mode = 'd',
        .skip_mp4 = false,
        .preserve_mtimes = false,
        .dual_mono = false
    },
    
    // Musepack config
    {
        .tag_mode = 'i',
        .skip_existing = false,
        .target_loudness = RG_TARGET_LOUDNESS,
        .max_peak_level = 0.0,
        .true_peak = false,
        .clip_mode = 'p',
        .do_album = true,
        .tab_output = OutputType::NONE,
        .sep_header = false,
        .sort_alphanum = false,
        .lowercase = false,
        .id3v2version = ID3V2_KEEP,
        .opus_mode = 'd',
        .skip_mp4 = false,
        .preserve_mtimes = false,
        .dual_mono = false
    }
};

const Config& get_config(FileType type)
{
    return configs[static_cast<int>(type)];
}

// Parse Easy Mode command line arguments
void easy_mode(int argc, char *argv[])
{
    int rc, i;
    char *preset = nullptr;
    const char *short_opts = "+hqSl:m:p:O::";
    unsigned int threads = 1;
    opterr = 0;

    static struct option long_opts[] = {
        { "help",          no_argument,       nullptr, 'h' },
        { "quiet",         no_argument,       nullptr, 'q' },

        { "skip-existing", no_argument,       nullptr, 'S' },
        { "multithread",   required_argument, nullptr, 'm' },
        { "preset",        required_argument, nullptr, 'p' },
        { "output",        optional_argument, nullptr, 'O' },
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

            case 'S':
                for (Config &config : configs)
                    config.skip_existing = true;
                break;
            
            case 'm':
                {
                    unsigned int max_threads = std::thread::hardware_concurrency();
                    if (!max_threads)
                        max_threads = 1;
                    if (MATCH(optarg, "MAX") || MATCH(optarg, "max")) {
                        threads = max_threads;
                    }
                    else {
                        threads = (unsigned int) (strtoul(optarg, nullptr, 10));
                        if (threads < 1) {
                            output_fail("Invalid multithread argument '{}'", optarg);
                            quit(EXIT_FAILURE);
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
                if (preset == nullptr)
                    preset = optarg;
                break;

            case 'O':
                if (optarg) {
                    const auto& [sep_header, sort_alphanum] = parse_output_mode(optarg);
                    for (Config &config : configs) {
                        config.sep_header = sep_header;
                        config.sort_alphanum = sort_alphanum;
                    }
                }
                for (Config &config : configs)
                    config.tab_output = OutputType::FILE;
                break;

            case '?':
                if (optopt)
                    output_fail("Unrecognized option '{:c}'", optopt);
                else
                    output_fail("Unrecognized option '{}'", argv[optind - 1] + 2);
                quit(EXIT_FAILURE);
        }
    }

    if (argc == optind) {
        output_fail("No directory specified");
        quit(EXIT_FAILURE);
    }

    scan_easy(argv[optind], preset ? preset : std::filesystem::path(), threads);
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
    output_fail("'{}' is not a valid boolean", value);
    return false;
}

static FileType determine_section_type(const std::string &section)
{
    static const std::unordered_map<std::string, FileType> map = {
        {"MP2",      FileType::MP2},
        {"MP3",      FileType::MP3},
        {"FLAC",     FileType::FLAC},
        {"Ogg",      FileType::OGG},
        {"Opus",     FileType::OPUS},
        {"M4A",      FileType::M4A},
        {"WMA",      FileType::WMA},
        {"WAV",      FileType::WAV},
        {"AIFF",     FileType::AIFF},
        {"Wavpack",  FileType::WAVPACK},
        {"APE",      FileType::APE},
        {"TAK",      FileType::TAK},
        {"Musepack", FileType::MPC}
    };
    auto it = map.find(section);
    return it == map.end() ? FileType::INVALID : it->second;
}

// Callback for INI parser
int global_handler([[maybe_unused]] void *user, const char *section, const char *name, const char *value)
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
        else
            quit(EXIT_FAILURE);
    }
    else if (MATCH(name, "TagMode")) {
        char tag_mode;
        if (parse_tag_mode_easy(value, tag_mode)) {
            for (Config &config : configs)
                config.tag_mode = tag_mode;
        }
        else
            quit(EXIT_FAILURE);
    }
    else if (MATCH(name, "ClipMode")) {
        char clip_mode;
        if (parse_clip_mode(value, clip_mode)) {
            for (Config &config : configs)
                config.clip_mode = clip_mode;
        }
        else
            quit(EXIT_FAILURE);
    }
    else if (MATCH(name, "TargetLoudness")) {
        double target_loudness;
        if (parse_target_loudness(value, target_loudness)) {
            for (Config &config : configs)
                config.target_loudness = target_loudness;
        }
        else
            quit(EXIT_FAILURE);
    }
    else if (MATCH(name, "MaxPeakLevel")) {
        double max_peak_level;
        if (parse_max_peak_level(value, max_peak_level)) {
            for (Config &config : configs)
                config.max_peak_level = max_peak_level;
        }
        else
            quit(EXIT_FAILURE);
    }
    else if (MATCH(name, "TruePeak")) {
        bool true_peak;
        if (convert_bool(value, true_peak)) {
            for (Config &config : configs)
                config.true_peak = true_peak;
        }
        else
            quit(EXIT_FAILURE);
    }
    else if (MATCH(name, "Lowercase")) {
        bool lowercase;
        if (convert_bool(value, lowercase)) {
            for (Config &config : configs)
                config.lowercase = lowercase;
        }
        else
            quit(EXIT_FAILURE);
    }
    else if (MATCH(name, "ID3v2Version")) {
        unsigned int id3v2version;
        if (parse_id3v2_version(value, id3v2version)) {
            for (Config &config : configs)
                config.id3v2version = id3v2version;
        }
        else
            quit(EXIT_FAILURE);
    }
    else if (MATCH(name, "OpusMode")) {
        char opus_mode;
        if (parse_opus_mode(value, opus_mode)) {
            for (Config &config : configs)
                config.opus_mode = opus_mode;
        }
        else
            quit(EXIT_FAILURE);
    }
    else if (MATCH(name, "PreserveMtimes")) {
        bool preserve_mtimes;
        if (convert_bool(value, preserve_mtimes)) {
            for (Config &config : configs)
                config.preserve_mtimes = preserve_mtimes;
        }
        else
            quit(EXIT_FAILURE);
    }
    else if (MATCH(name, "DualMono")) {
        bool dual_mono;
        if (convert_bool(value, dual_mono)) {
            for (Config &config : configs)
                config.dual_mono = dual_mono;
        }
        else
            quit(EXIT_FAILURE);
    }
    return 0;
}

int format_handler([[maybe_unused]] void *user, const char *section, const char *name, const char *value)
{
    FileType file_type = determine_section_type(section);
    if (file_type == FileType::INVALID)
        return 0;

    // Parse setting keys
    if (MATCH(name, "Album"))
        convert_bool(value, configs[static_cast<int>(file_type)].do_album);
    else if (MATCH(name, "TagMode"))
        parse_tag_mode_easy(value, configs[static_cast<int>(file_type)].tag_mode);
    else if (MATCH(name, "ClipMode"))
        parse_clip_mode(value, configs[static_cast<int>(file_type)].clip_mode);
    else if (MATCH(name, "Lowercase"))
        convert_bool(value, configs[static_cast<int>(file_type)].lowercase);
    else if (MATCH(name, "ID3v2Version"))
        parse_id3v2_version(value, configs[static_cast<int>(file_type)].id3v2version);
    else if (MATCH(name, "TargetLoudness"))
        parse_target_loudness(value, configs[static_cast<int>(file_type)].target_loudness);
    else if (MATCH(name, "MaxPeakLevel"))
        parse_max_peak_level(value, configs[static_cast<int>(file_type)].max_peak_level);
    else if (MATCH(name, "TruePeak"))
        convert_bool(value, configs[static_cast<int>(file_type)].true_peak);
    else if (MATCH(name, "OpusMode"))
        parse_opus_mode(value, configs[static_cast<int>(file_type)].opus_mode);
    else if (file_type == FileType::M4A && MATCH(name, "SkipMP4"))
        convert_bool(value, configs[static_cast<int>(file_type)].skip_mp4);
    else if (MATCH(name, "PreserveMtimes"))
        convert_bool(value, configs[static_cast<int>(file_type)].preserve_mtimes);
    else if (MATCH(name, "DualMono"))
        convert_bool(value, configs[static_cast<int>(file_type)].dual_mono);
    return 0;
}

inline bool join_path([[maybe_unused]] const std::filesystem::path &p)
{
    return true;
}

template<typename... Args>
inline bool join_path(std::filesystem::path &path, const std::filesystem::path &first, const Args&... args)
{
    path /= first;
    return join_path(path, args...);
}

template<typename... Args>
inline std::filesystem::path join_paths(const std::filesystem::path &first, const Args&... args)
{    
    std::filesystem::path path(first);
    return join_path(path, args...) ? path : std::filesystem::path();
}

static void load_preset(const std::filesystem::path &preset)
{
    std::filesystem::path path(preset);

    // Find preset file from name
    if (!path.has_extension()) {
        std::filesystem::path preset_basename(std::move(path));
        preset_basename += ".ini";

        // Check user directory
#ifdef _WIN32
        char buffer[1024];
        if (GetEnvironmentVariableA("USERPROFILE", buffer, sizeof(buffer)))
            path = join_paths(buffer, "." EXECUTABLE_TITLE, "presets", preset_basename);
#else
        const char* const home = getenv("HOME");
        if (home)
#ifdef __APPLE__
            path = join_paths(home, "Library", EXECUTABLE_TITLE, "presets", preset_basename);
#else
            path = join_paths(home, ".config", EXECUTABLE_TITLE, "presets", preset_basename);
#endif
#endif

        // Check .exe preset folder on Windows, system directory on Unix
        if (!std::filesystem::exists(path)) {
#ifdef _WIN32
            if (GetModuleFileNameA(nullptr, buffer, sizeof(buffer))) {
                std::filesystem::path exe(buffer);
                path = join_paths(exe.parent_path(), "presets", preset_basename);
            }
#else
            path = join_paths(PRESETS_DIR, preset_basename);
#endif
        }
    }

    if (!std::filesystem::exists(path)) {
        output_error("Could not locate preset '{}'", preset.string());
        quit(EXIT_FAILURE);
    }

    // Parse file
    std::FILE *file = fopen(path.string().c_str(), "r");
    if (!file) {
        output_error("Failed to open preset from '{}'", path.string());
        quit(EXIT_FAILURE);
    }

    output_ok("Applying preset '{}'...", preset.string());
    ini_parse_file(file, global_handler, nullptr);
    rewind(file);
    ini_parse_file(file, format_handler, nullptr);
    fclose(file);
}

bool WorkerThread::place_job(std::unique_ptr<ScanJob> &job)
{
    std::unique_lock lock(mutex, std::try_to_lock);
    if (!lock.owns_lock())
        return false;
    
    this->job = std::move(job);
    job_available = true;
    cv.notify_all();
    return true;
}

void WorkerThread::work()
{
    std::unique_lock lock(mutex);
    {
        std::scoped_lock main_lock(main_mutex);
        main_cv.notify_all();
    }

    while (!quit) {
        if (job_available) {
            job->scan(&ffmpeg_mutex);
            
            // Update statistics
            {
                std::scoped_lock main_lock(main_mutex);
                job->update_data(data);
            }
            job_available = false;
            main_cv.notify_all();
        }
                                                                                                                                                                                                                                                                                                                                                                        
        // Wait until we get a new job from the main thread
        cv.wait_for(lock, std::chrono::seconds(MAX_THREAD_SLEEP));
    }
}

bool WorkerThread::wait()
{
    {
        std::unique_lock lock(mutex, std::try_to_lock);
        if (!lock.owns_lock()) 
            return false;
        quit = true;
        cv.notify_all();
    }

    thread->join();
    return true;
}

void scan_easy(const std::filesystem::path &path, const std::filesystem::path &preset, size_t nb_threads)
{
    std::queue<std::unique_ptr<ScanJob>> jobs;
    ScanData data;

    // Verify directory exists and is valid
    if (!std::filesystem::exists(path)) {
        output_fail("Directory '{}' does not exist", path.string());
        quit(EXIT_FAILURE);
    }
    else if (!std::filesystem::is_directory(path)) {
        output_fail("'{}' is not a valid directory", path.string());
        quit(EXIT_FAILURE);
    }

    // Load scan preset
    if (!preset.empty())
        load_preset(preset);

    // Record start time
    const auto start_time = std::chrono::system_clock::now();

    // Generate queue of all directories in directory tree
    output_ok("Building directory tree...");
    std::queue<std::filesystem::path> directories;
    directories.emplace(path);
    for (const std::filesystem::directory_entry &entry : std::filesystem::recursive_directory_iterator(path)) {
        if (entry.is_directory())
            directories.emplace(entry.path());
    }
    size_t nb_directories = directories.size();
    output_ok("Found {:L} {}...", nb_directories, nb_directories > 1 ? "directories" : "directory");
    output_ok("Scanning {} for files...", nb_directories > 1 ? "directories" : "directory");
    ScanJob *job;
    while(!directories.empty()) {
        if ((job = ScanJob::factory(directories.front())))
            jobs.emplace(job);
        directories.pop();
    }
    size_t nb_jobs = jobs.size();
    if (nb_threads > nb_jobs)
        nb_threads = nb_jobs;

    // Mulithreaded scanning
    if (nb_threads > 1) {
        MTProgress progress(nb_jobs);
        std::vector<std::unique_ptr<WorkerThread>> threads;
        std::mutex ffmpeg_mutex;
        std::mutex mutex;
        std::condition_variable cv;
        std::unique_lock lock(mutex);

        // Spawn worker threads
        output_ok("Scanning with {} threads...", nb_threads);
        for (size_t i = 0; i < nb_threads; i++) {
            progress.update(jobs.front()->path.string());
            threads.emplace_back(std::make_unique<WorkerThread>(
                jobs.front(),
                mutex,
                ffmpeg_mutex,
                cv,
                data
            ));
            cv.wait_for(lock, std::chrono::milliseconds(200));
            jobs.pop();
        }

        // Feed jobs to workers
        std::string current_job;
        if (!jobs.empty())
            current_job = jobs.front()->path.string();
        while (!jobs.empty()) {
            cv.wait_for(lock, std::chrono::milliseconds(200));
            for (auto &thread : threads) {
                if (thread->place_job(jobs.front())) {
                    jobs.pop();
                    progress.update(current_job);
                    if (!jobs.empty())
                        current_job = jobs.front()->path.string();
                    break;
                }
            }
        }
        cv.wait_for(lock, std::chrono::milliseconds(200));

        // All jobs have been placed, wait for threads to finish scanning
        while (1) {
            for (auto thread = threads.begin(); thread != threads.end();)
                thread = (*thread)->wait() ? threads.erase(thread) : thread + 1;
            if (threads.empty())
                break;
            cv.wait_for(lock, std::chrono::milliseconds(200));
        }
        rsgain::print("\33[2K\n");
    }

    // Single threaded scanning
    else {
        while (!jobs.empty()) {
            auto &job = jobs.front();
            job->scan();
            job->update_data(data);
            jobs.pop();
        }
        rsgain::print("\n");
    }

    // Output statistics at the end
    auto duration = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now() - start_time);
    if (!data.files) {
        if (data.skipped)
            rsgain::print("Skipped {:L} file{} with existing ReplayGain information\n",
                data.skipped,
                data.skipped > 1 ? "s" : ""
            );
        rsgain::print("No files were scanned\n");
        return;
    }
    
    rsgain::print(COLOR_GREEN "Scanning Complete" COLOR_OFF "\n");
    HELP_STATS("Time Elapsed", "{:%H:%M:%S}", duration);
    HELP_STATS("Files Scanned", "{:L}", data.files);
    if (data.skipped)
        HELP_STATS("Files Skipped", "{:L}", data.skipped);
    HELP_STATS("Clip Adjustments", "{:L} ({:.1f}% of files)", data.clipping_adjustments, 100.f * (float) data.clipping_adjustments / (float) data.files);
    HELP_STATS("Average Loudness", "{:.2f} LUFS", data.total_loudness / (double) data.files);
    HELP_STATS("Average Gain", "{:.2f} dB", data.total_gain / (double) data.files);
    double average_peak = data.total_peak / (double) data.files;
    HELP_STATS("Average Peak", "{:.6f}{}", average_peak, average_peak != 0.0 ? rsgain::format(" ({:.2f} dB)", 20.0 * log10(average_peak)) : "");
    HELP_STATS("Negative Gains", "{:L} ({:.1f}% of files)", data.total_negative, 100.f * (float) data.total_negative / (float) data.files);
    HELP_STATS("Positive Gains", "{:L} ({:.1f}% of files)", data.total_positive, 100.f * (float) data.total_positive / (float) data.files);
    rsgain::print("\n");

    // Inform user of errors
    if (!data.error_directories.empty()) {
        rsgain::print(COLOR_RED "There were errors while scanning the following directories:" COLOR_OFF "\n");
        for (const std::string &s : data.error_directories)
            rsgain::print("{}\n", s);
        rsgain::print("\n");
    }
}

static inline void help_easy() {
    rsgain::print(COLOR_RED "Usage: " COLOR_OFF "{}{}{} easy [OPTIONS] DIRECTORY\n", COLOR_GREEN, EXECUTABLE_TITLE, COLOR_OFF);

    rsgain::print("  Easy Mode recursively scans a directory using the recommended settings for each\n");
    rsgain::print("  file type. Easy Mode assumes that you have your music library organized with each album\n");
    rsgain::print("  in its own folder.\n");

    rsgain::print("\n");
    rsgain::print(COLOR_RED "Options:\n" COLOR_OFF);

    CMD_HELP("--help",     "-h", "Show this help");
    CMD_HELP("--quiet",      "-q",  "Don't print scanning status messages");
    rsgain::print("\n");

    CMD_HELP("--skip-existing", "-S", "Don't scan files with existing ReplayGain information");
    CMD_HELP("--multithread=n", "-m n", "Scan files with n parallel threads");
    CMD_HELP("--preset=s", "-p s", "Load scan preset s");

    rsgain::print("\n");

    CMD_HELP("--output", "-O",  "Output tab-delimited scan data to CSV file per directory");
    CMD_HELP("--output=s", "-O s",  "Output with sep header (needed for Microsoft Excel compatibility)");
    CMD_HELP("--output=a", "-O a",  "Output with files sorted in alphanumeric order");

    rsgain::print("\n");

    rsgain::print("Please report any issues to " PROJECT_URL "/issues\n");
    rsgain::print("\n");
}
