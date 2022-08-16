#include <filesystem>
#include <vector>
#include <deque>
#include <chrono>
#include <set>
#include <string>
#include <thread>
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

static const struct extension_type extensions[] {
    {".mp2",  MP2},
    {".mp3",  MP3},
    {".flac", FLAC},
    {".ogg",  OGG},
    {".oga",  OGG},
    {".spx",  OGG},
    {".opus", OPUS},
    {".m4a",  M4A},
    {".wma",  WMA},
    {".wav",  WAV},
    {".aiff", AIFF},
    {".aif",  AIFF},
    {".snd",  AIFF},
    {".wv",   WAVPACK},
    {".ape",  APE}
};

// Default configs
static Config configs[] = {
    
    // MP2 config
    {
        .mode = 'e',
        .unit = UNIT_DB,
        .pre_gain = 0.0f,
        .max_true_peak_level = -1.0f,
        .no_clip = true,
        .warn_clip = true,
        .do_album = true,
        .tab_output = false,
        .lowercase = true,
        .strip = true,
        .id3v2version = 3
    },

    // MP3 config
    {
        .mode = 'e',
        .unit = UNIT_DB,
        .pre_gain = 0.0f,
        .max_true_peak_level = -1.0f,
        .no_clip = true,
        .warn_clip = true,
        .do_album = true,
        .tab_output = false,
        .lowercase = true,
        .strip = true,
        .id3v2version = 3
    },

    // FLAC config
    {
        .mode = 'e',
        .unit = UNIT_DB,
        .pre_gain = 0.0f,
        .max_true_peak_level = -1.0f,
        .no_clip = true,
        .warn_clip = true,
        .do_album = true,
        .tab_output = false,
        .lowercase = false,
        .strip = false,
        .id3v2version = 4
    },

    // OGG config
    {
        .mode = 'e',
        .unit = UNIT_DB,
        .pre_gain = 0.0f,
        .max_true_peak_level = -1.0f,
        .no_clip = true,
        .warn_clip = true,
        .do_album = true,
        .tab_output = false,
        .lowercase = false,
        .strip = false,
        .id3v2version = 4
    },

    // OPUS config
    {
        .mode = 'e',
        .unit = UNIT_DB,
        .pre_gain = 0.0f,
        .max_true_peak_level = -1.0f,
        .no_clip = true,
        .warn_clip = true,
        .do_album = true,
        .tab_output = false,
        .lowercase = false,
        .strip = false,
        .id3v2version = 4
    },

    // M4A config
    {
        .mode = 'e',
        .unit = UNIT_DB,
        .pre_gain = 0.0f,
        .max_true_peak_level = -1.0f,
        .no_clip = true,
        .warn_clip = true,
        .do_album = true,
        .tab_output = false,
        .lowercase = true,
        .strip = false,
        .id3v2version = 4
    },

    // WMA config
    {
        .mode = 'e',
        .unit = UNIT_DB,
        .pre_gain = 0.0f,
        .max_true_peak_level = -1.0f,
        .no_clip = true,
        .warn_clip = true,
        .do_album = true,
        .tab_output = false,
        .lowercase = true,
        .strip = false,
        .id3v2version = 4
    },

    // WAV config
    {
        .mode = 'e',
        .unit = UNIT_DB,
        .pre_gain = 0.0f,
        .max_true_peak_level = -1.0f,
        .no_clip = true,
        .warn_clip = true,
        .do_album = true,
        .tab_output = false,
        .lowercase = true,
        .strip = false,
        .id3v2version = 3
    },

    // AIFF config
    {
        .mode = 'e',
        .unit = UNIT_DB,
        .pre_gain = 0.0f,
        .max_true_peak_level = -1.0f,
        .no_clip = true,
        .warn_clip = true,
        .do_album = true,
        .tab_output = false,
        .lowercase = true,
        .strip = false,
        .id3v2version = 3
    },

    // WAVPACK config
    {
        .mode = 'e',
        .unit = UNIT_DB,
        .pre_gain = 0.0f,
        .max_true_peak_level = -1.0f,
        .no_clip = true,
        .warn_clip = true,
        .do_album = true,
        .tab_output = false,
        .lowercase = false,
        .strip = true,
        .id3v2version = 4
    },

    // APE config
    {
        .mode = 'e',
        .unit = UNIT_DB,
        .pre_gain = 0.0f,
        .max_true_peak_level = -1.0f,
        .no_clip = true,
        .warn_clip = true,
        .do_album = true,
        .tab_output = false,
        .lowercase = false,
        .strip = true,
        .id3v2version = 4
    },
};

// A function to determine a file type
static FileType determine_filetype(const char *extension)
{
    for (const struct extension_type &ext : extensions) {
        if (MATCH(extension, ext.extension)) {
            return ext.file_type;
        }
    }
    return INVALID;
}

// A function to determine if a given file is a given type
static bool is_type(const char *extension, const FileType file_type)
{
    for (const struct extension_type &ext : extensions) {
        if (MATCH(extension, ext.extension)) {
            if (ext.file_type == file_type) {
                return true;
            }
            else {
                return false;
            }
        }
    }
    return false;
}

static void convert_bool(const char *value, bool &setting)
{
    if(MATCH(value, "True") || MATCH(value, "true")) {
        setting = true;
    }
    else if (MATCH(value, "False") || MATCH(value, "false")) {
        setting = false;
    }
}

static FileType determine_section_type(const char *section)
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

    for (const struct overrides_section &s : sections) {
        if(MATCH(section, s.section_name)) {
            return s.file_type;
        }
    }
    return INVALID;
}

// Callback for INI parser
static int handler(void *user, const char *section, const char *name, const char *value)
{
    FileType file_type = determine_section_type(section);
    if (file_type == INVALID) {
        return 0;
    }

    // Parse setting keys
    if (MATCH(name, "AlbumGain")) {
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
        parse_id3v2version(value, configs[file_type]);
    }
    else if (MATCH(name, "Pregain")) {
        parse_pregain(value, configs[file_type]);
    }
    else if (MATCH(name, "MaxTruePeakLevel")) {
        parse_max_true_peak_level(value, configs[file_type]);
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

void copy_string_alloc(char **dest, const char *string)
{
  int length = strlen(string);
  if (length) {
    *dest = (char*) malloc(sizeof(char)*(length + 1));
    strcpy(*dest, string);
  }
  else {
    *dest = NULL;
  }
}

Job::Job(const std::vector<std::u8string> &file_list, const FileType file_type, const std::filesystem::path &path) : config(configs[file_type]){
    nb_files = file_list.size();
    files = (char**) malloc(sizeof(char*) * nb_files);
    for (int i = 0; i < nb_files; i++) {
        copy_string_alloc(files + i, (char*) file_list[i].c_str());
    }
    directory = path.u8string();
}
Job::~Job() {
    for (int i = 0; i < nb_files; i++) {
        free(files[i]);
    }
    free(files);
}

// Generates a scan job from a directory path
Job *generate_job(const std::filesystem::path &path)
{
    std::set<FileType> extensions;
    std::vector<std::u8string> file_list;
    FileType file_type;
    size_t num_extensions;

    // Determine directory filetype
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(path)) {
        if (!entry.is_regular_file() || !entry.path().has_extension()) {
            continue;
        }
        file_type = determine_filetype((char*) entry.path().extension().u8string().c_str());
        if (file_type != INVALID) {
            extensions.insert(file_type);
        }
    }
    num_extensions = extensions.size();
    if (num_extensions != 1) {
        return NULL;
    }
    file_type = *extensions.begin();

    // Generate vector of files with directory file type
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(path)) {
        if (!entry.is_regular_file() || !entry.path().has_extension()) {
            continue;
        }
        if (is_type((char*) entry.path().extension().u8string().c_str(), file_type)) {
            file_list.push_back(entry.path().u8string());
        }
    }
    return new Job(file_list, file_type, path);
}

WorkerThread::WorkerThread(std::mutex *ffmpeg_mutex, std::mutex *main_mutex, std::condition_variable *main_cv)
{
    this->ffmpeg_mutex = ffmpeg_mutex;
    this->main_mutex = main_mutex;
    this->main_cv = main_cv;
    files = NULL;
    quit = false;
    thread = new std::thread(&WorkerThread::work, this);
}

void WorkerThread::add_job(int start_index, int nb_files, char **files)
{
    std::unique_lock lk(this->thread_mutex);
    this->start_index = start_index;
    this->nb_files = nb_files;
    this->files = files;
    lk.unlock();
    thread_cv.notify_all();
}

WorkerThread::~WorkerThread()
{
    delete thread;
}

void WorkerThread::work()
{
    std::unique_lock main_lock(*main_mutex, std::defer_lock);
    std::unique_lock thread_lock(thread_mutex);

    while (!quit) {
        if (files != NULL) {
            error = false;
            int end = start_index + nb_files;
            finished = false;
            for(int i = start_index; i < end && !error; i++) {
                error = scan_file(files[i], i, ffmpeg_mutex);
            }
            files = NULL;
            finished = true;

            // Inform the main thread that the scanning is finished
            main_lock.lock();
            main_cv->notify_all();
            main_lock.unlock();
        }
                                                                                                                                                                                                                                                                                                                                                                        
        // Wait until we get a new job from the main thread
        thread_cv.wait_for(thread_lock, std::chrono::seconds(MAX_THREAD_SLEEP));
    }

    return;
}

bool WorkerThread::is_finished(bool &error)
{
    std::unique_lock lk(thread_mutex, std::try_to_lock);
    if (!lk.owns_lock())
        return false;
    bool finished = this->finished;
    if (this->finished)
        this->finished = false;
    error |= this->error;
    lk.unlock();
    return finished;   
}

void WorkerThread::quit_thread()
{
    std::unique_lock lk(thread_mutex);
    quit = true;
    lk.unlock();
    thread_cv.notify_all();
    thread->join();
}

void scan_easy(const char *directory, const char *overrides_file)
{
    std::filesystem::path path((char8_t*) directory);
    std::vector<std::u8string> error_directories;
    bool error;
    Job *job;
    long total_files = 0;

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
    output_ok("Building directory tree");
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
        std::vector<WorkerThread*> active_threads;
        std::mutex ffmpeg_mutex;
        std::mutex main_mutex;
        std::unique_lock main_lock(main_mutex);
        std::condition_variable main_cv;

        // Create threads
        for (int i = 0; i < multithread; i++) {
            worker_threads.push_back(new WorkerThread(&ffmpeg_mutex, &main_mutex, &main_cv));
        }

        while (directories.size()) {
            error = false;
            job = generate_job(directories.front());
            directories.pop_front();
            if (job == NULL)
                continue;

            output_ok("Scanning directory: {}", (char*) job->directory.c_str());
            scan_init(job->nb_files);

            // Distribute the files evenly amongst the worker threads
            int full_thread = job->nb_files / multithread;
            if (job->nb_files % multithread)
                full_thread++;
            int thread_nb_files;
            active_threads.clear();
            for (int i = 0; i*full_thread < job->nb_files; i++) {
                thread_nb_files = (i*full_thread + full_thread < job->nb_files) ? full_thread : job->nb_files - i*full_thread;
                worker_threads[i]->add_job(i*full_thread, thread_nb_files, job->files);
                active_threads.push_back(worker_threads[i]);
            }

            // Wait for threads to finish scanning
            int num_active = active_threads.size();
            while (num_active) {
                main_cv.wait_for(main_lock, std::chrono::seconds(MAX_THREAD_SLEEP));
                for (WorkerThread *at : active_threads) {
                    if (at->is_finished(error)) {
                        num_active--;
                    }
                }
            }
            
            // Make gain calculations and tag the files
            if (!error) {
                apply_gain(job->nb_files, job->files, job->config);
                total_files += job->nb_files;
            }
            else {
                error_directories.push_back(job->directory);
            }

            scan_deinit();
            delete job;
        }

        // Quit threads after all jobs have been completed
        for(WorkerThread *wt : worker_threads) {
            wt->quit_thread();
            delete wt;
        }
    }

    // Single threaded scanning
    else {
        while (directories.size()) {
            job = generate_job(directories.front());
            directories.pop_front();
            if (job == NULL)
                continue;

            output_ok("Scanning directory: {}", (char*) job->directory.c_str());
            error = scan(job->nb_files, job->files, job->config);
            if (error) {
                error_directories.push_back(job->directory);
            }
            else {
                total_files += job->nb_files;
            }
            delete job;
        }
    }

    const auto end_time = std::chrono::system_clock::now();
    if (!total_files) {
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

    // Format file total string
    std::stringstream ss;
    ss.imbue(std::locale(""));
    ss << std::fixed << total_files;
    
    fmt::print(COLOR_YELLOW "Files Scanned:" COLOR_OFF " {}\n", ss.str().c_str());
#if CALC_TIME
    fmt::print(COLOR_YELLOW "Time Elapsed:" COLOR_OFF "  {}\n", time_string.c_str());
#endif
    fmt::print("\n");

    // Inform user of errors
    if (error_directories.size()) {
        fmt::print(COLOR_RED "There were errors while scanning the following directories:" COLOR_OFF "\n");
        for (std::u8string &d : error_directories) {
            fmt::print("{}\n", (char*) d.c_str());
        }
        fmt::print("\n");
    }
}