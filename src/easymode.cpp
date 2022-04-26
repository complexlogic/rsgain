#include <filesystem>
#include <vector>
#include <stdio.h>
#include <set>
#include <string.h>
#include <string>
#include "loudgain.h"
#include "easymode.h"
#include "output.h"
using namespace std;

typedef enum {
    INVALID = -1,
    MP3,
    FLAC,
    OGG,
    OPUS,
    M4A,
    WMA,
    WAV,
    AIFF,
    WAVPACK,
    APE
} FileType;

struct extension_type {
    const char *extension;
    FileType file_type;
};

static const struct extension_type extensions[] {
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
    {".wv",   WAVPACK},
    {".ape",  APE}
};

// Default configs
static Config configs[] = {
    
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
		.tab_output_new = false,
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
		.tab_output_new = false,
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
		.tab_output_new = false,
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
		.tab_output_new = false,
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
		.tab_output_new = false,
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
		.tab_output_new = false,
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
		.tab_output_new = false,
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
		.tab_output_new = false,
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
		.tab_output_new = false,
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
		.tab_output_new = false,
		.lowercase = false,
		.strip = true,
		.id3v2version = 4
    },
};

// A function to determine a file type
static FileType determine_type(const char *extension)
{
    for (const struct extension_type &ext : extensions) {
        if (!strcmp(extension, ext.extension)) {
            return ext.file_type;
        }
    }
    return INVALID;
}

// A function to determine if a given file is a given type
static bool is_type(const char *extension, const FileType file_type)
{
    for (const struct extension_type &ext : extensions) {
        if (!strcmp(extension, ext.extension)) {
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

void scan_easy(const char *directory)
{
    filesystem::path path(directory);
    filesystem::directory_entry dir(path);
    if (!dir.exists()) {
        output("Error: Directory \"%s\" does not exist\n", directory);
        quit(EXIT_FAILURE);
    }
    else if (!dir.is_directory()) {
        output("Error: \"%s\" is not a valid directory\n", directory);
        quit(EXIT_FAILURE);
    }
    
    // Get vector of all subdirectories
    vector<filesystem::path> directories;
    directories.push_back(path);
    for (const filesystem::directory_entry &entry : filesystem::recursive_directory_iterator(path)) {
        if (entry.is_directory()) {
            directories.push_back(entry.path());
        }
    }

    set<FileType> extensions;
    vector<u8string> file_list;
    FileType file_type;
    size_t num_extensions;
    int nb_files;
    char **files = NULL;

    // Walk all subdirectories
    for (const filesystem::path &path : directories) {
        extensions.clear();
        file_list.clear();

        // Determine directory filetype
        for (const filesystem::directory_entry &entry : filesystem::directory_iterator(path)) {
            if (!entry.is_regular_file() || !entry.path().has_extension()) {
                continue;
            }
            file_type = determine_type((char*) entry.path().extension().u8string().c_str());
            if (file_type != INVALID) {
                extensions.insert(file_type);
            }
        }
        num_extensions = extensions.size();
        if (num_extensions != 1) {
            continue;
        }
        file_type = *extensions.begin();

        // Generate vector of files with directory file type
        for (const filesystem::directory_entry &entry : filesystem::directory_iterator(path)) {
            if (!entry.is_regular_file() || !entry.path().has_extension()) {
                continue;
            }
            if (is_type((char*) entry.path().extension().u8string().c_str(), file_type)) {
                file_list.push_back(entry.path().u8string());
            }
        }

        // Convert to array of C-style strings
        nb_files = file_list.size();
        files = (char**) malloc(sizeof(char*) * nb_files);
        for (int i = 0; i < nb_files; i++) {
            copy_string_alloc(files + i, (char*) file_list[i].c_str());
        }

        // Scan directory
        scan(nb_files, files, configs + file_type);

        // Free resources
        for (int i = 0; i < nb_files; i++) {
            free(files[i]);
        }
        free(files);
        files = NULL;
    }
}