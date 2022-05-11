# rsgain
**rsgain** (**r**eally **s**imple **gain**) is a ReplayGain 2.0 tagging utility for Windows, macOS, and Linux. rsgain applies loudness metadata tags to your files, while leaving the audio stream untouched. A ReplayGain-compatible player will dynamically adjust the volume of your tagged files during playback.

rsgain is a heavily modified fork of [loudgain](https://github.com/Moonbase59/loudgain). The goal of rsgain is to take the excellent platform created by loudgain and simplify it for the average user, while also preserving the advanced features for users that need them. The following improvements have been made from loudgain:
- Native Windows support
- Built-in recursive directory scanning without the need for a wrapper script. See [Easy Mode](#easy-mode) for more details.
- Multithreaded scanning

## Installation
Binary packages are provided on the [Release Page](https://github.com/complexlogic/rsgain/releases). You can also build the program youself, see [BUILDING](docs/BUILDING.md).

### Windows
rsgain is compatible with Windows 10 and later. Download the win64 .zip file from the [latest release](https://github.com/complexlogic/rsgain/releases/latest), and extract its contents to a directory of your choosing. 

It is recommended to add the directory to your `Path` system environment variable so you can invoke the program with the `rsgain` command instead of using the full path to its .exe file. In the Windows taskbar search, type "env", then select "Edit the system environment variables". In the resulting window, click the "Environment variables" button. In the next window under "System variables", select "Path", then press Edit. Add the folder that you extracted `rsgain.exe` to in the previous step.

### macOS
There is currently no binary package available for macOS, so Mac users will need to build from source. See [BUILDING](docs/BUILDING.md).

If anybody is willing to maintain a Homebrew package for rsgain, please reach out by making an issue on the  [Issue Tracker](https://github.com/complexlogic/rsgain/issues).

### Linux
An amd64 .deb package is provided on the [release page](https://github.com/complexlogic/rsgain/releases/latest). It is installable on most APT-based distro releases from 2020 and later.

There is also a PKGBUILD script for Arch/Manjaro users. Run the following commands from a clean directory to install:
```
wget https://raw.githubusercontent.com/complexlogic/rsgain/master/config/PKGBUILD
makepkg -si
```

Users of other distros will need to build from source. See [BUILDING](docs/BUILDING.md).

## Usage
rsgain has two modes of operation: Easy Mode and Custom Mode. Easy Mode is recommended over Custom Mode for almost all use cases. Custom Mode exists to provides backwards compatibility with the legacy loudgain/mp3gain command line.

### Easy Mode
Easy Mode recursively scans your entire music library using the recommended settings for each file type. You can use Easy Mode if the following conditions apply:
- Your music library is organized by album i.e. each album has its own folder
- In each album folder, all audio files are of the same type. It is acceptable to have non-audio files mixed in such as log files or artwork, but if multiple *audio* file types are detected, the folder will not be scanned.

Easy Mode is invoked with the command `rsgain easy` followed by the root of the directory you want to scan:
```
rsgain easy /path/to/music/library
```
```
rsgain easy "C:\path\to\music libary"
```
That's it. rsgain will take care of the details. See [Overrides](#overriding-default-settings) for more information about the default settings and how to override them, if desired.

#### Multithreaded Scanning
Easy Mode includes optional multithreaded operation to speed up the duration of a scan. Use the `-m` option, followed by the number of threads to create. The number of threads must be one per physical CPU core or fewer. For example, if you have a quad core CPU:
```
rsgain easy -m 4 /path/to/music/library
``` 
The scan progress bar is automatically disabled in multithreaded operation because multiple files are being scanned in parallel. 

The speed gains offered by multithreaded scanning are significant. On my library of about 12,000 songs, it takes about 5 hours in the default single threaded mode, but only 1 hour 45 minutes hours with `-m 4`, a reduction of about 65%.

Expect close to 100% CPU utilization when scanning multithreaded. I typically use multithreaded on a full library scan, and regular single threaded mode when incrementally adding 1 or 2 albums to my library.

#### Overriding Default Settings
Easy Mode scans files with the following settings by default:
- Clipping protection enabled
- Lowercase tags for MP2/MP3, M4A, WMA, and AIFF (uppercase for all others)
- Decibal units
- All optional tags enabled:
	+ Range calculations
	+ Peak calcuations
	+ Reference loudness
	+ Album tags
- ID3 version 2.3 tags
- Strip obsolete ID3 version 1 tags
- -1 dBTP max true peak
- No pregain

These settings are recommended for maximum comptatibility with available players. However, if you need one of the settings changed, you can override them on a per-format basis using the `-o` option, and an overrides file.

The overrides file is an INI-formatted file that contains sections enclosed in square brackets which correspond to the available formats, and each section contains key=value pairs that correspond to settings. The overrides feature is intended for users that can't use the default settings of Easy Mode, but still prefer the functionality of Easy Mode over Custom Mode.

For example, rsgain writes lowercase tags on MP3 files by default, but you want uppercase tags instead. Format your `overrides.ini` file as follows:
```INI
[MP3]
Lowercase=false
```
Then, pass the path to the overrides file with the `-o` option:

```
rsgain easy -o /path/to/overrides.ini /path/to/music/library
```
A default `overrides.ini` file ships with rsgain in the root package directory for Windows, and in `<install prefix>/share/rsgain` for Unix. The file is pre-populated will all settings that are available to change. Note that this is an *overrides* file, not a configuration file, i.e any formats or settings you're not interested in can simply be deleted and the defaults will be used instead.

Each setting key corresponds to a command line option in Custom Mode. Below is a table of all settings available for override.

|Setting Key | Value Type | Custom Mode Option|
|------------|------------|-------------------|
|Mode|Character|-s|
|Lowercase|Boolean|-L|
|AlbumGain|Boolean|-a|
|ClippingProtection|Boolean|-k|
|ID3v2Version|Integer|-I|
|Strip|Boolean|-S|
|MaxTruePeakLevel|Decimal|-K|
|Pregain|Decimal|-d|

See the [Custom Mode help](#command-line-help) for more information.

### Custom Mode
Custom Mode preserves loudgain's command line syntax for users that still need it. Unlike Easy Mode, Custom Mode works with files, not directories. If you want recursive directory-based scanning, you will need to use a wrapper script.

Custom Mode is invoked with `rsgain custom` followed by options and a list of files to scan. For example, scan all FLAC files in the current directory with album gain and clipping protection enabled:
 ```
 rsgain custom -a -k -s e *.flac
 ```

#### Command Line Help:
```
Usage: rsgain custom [OPTIONS] FILES...
  Custom Mode allows the user to specify the options to scan the files with. The
  list of files to scan must be listed explicitly after the options.

Options:
  -h,   --help            Show this help.

  -r,   --track           Calculate track gain only (default).
  -a,   --album           Calculate album gain (and track gain).

  -c,   --clip            Ignore clipping warning.
  -k,   --noclip          Lower track/album gain to avoid clipping (<= -1 dBTP).
  -K n, --maxtpl=n        Avoid clipping; max. true peak level = n dBTP.
  -d n, --pregain=n       Apply n dB/LU pre-gain value (-5 for -23 LUFS target).

  -s d, --tagmode=d       Delete ReplayGain tags from files.
  -s i, --tagmode=i       Write ReplayGain 2.0 tags to files.
  -s e, --tagmode=e       like '-s i', plus extra tags (reference, ranges).
  -s l, --tagmode=l       like '-s e', but LU units instead of dB.
  -s s, --tagmode=s       Don't write ReplayGain tags (default).

  -L,   --lowercase       Force lowercase tags (MP2/MP3/MP4/WMA/WAV/AIFF).
                          This is non-standard but sometimes needed.
  -S,   --striptags       Strip tag types other than ID3v2 from MP2/MP3.
                          Strip tag types other than APEv2 from WavPack/APE.
  -I 3, --id3v2version=3  Write ID3v2.3 tags to MP2/MP3/WAV/AIFF.
  -I 4, --id3v2version=4  Write ID3v2.4 tags to MP2/MP3/WAV/AIFF (default).

  -O,   --output          Database-friendly tab-delimited list output.
  -q,   --quiet           Don't print scanning status messages.

Please report any issues to https://github.com/complexlogic/rsgain/issues
```