# rsgain

## Table of Contents

1. [About](#about)
2. [Installation](#installation)
3. [Supported File Formats](#supported-file-formats)
4. [Usage](#usage)
    - [Easy Mode](#easy-mode)
    - [Custom Mode](#custom-mode)
    - [MusicBrainz Picard Plugin](#musicbrainz-picard-plugin)
5. [Design Philosophy](#design-philosophy)
6. [License](#license)

## About

**rsgain** (**r**eally **s**imple **gain**) is a ReplayGain 2.0 command line utility for Windows, macOS, Linux, and BSD. rsgain applies loudness metadata tags to your files, while leaving the audio stream untouched. A ReplayGain-compatible player will dynamically adjust the volume of your tagged files during playback.

rsgain is designed with a "batteries included" philosophy, allowing a user to scan their entire music library without requiring external scripts or other tools. It aims to strike the perfect balance between power and simplicity by providing multiple user interfaces. See [Usage](#usage) for more information.

rsgain is the backend for the [MusicBrainz Picard](https://picard.musicbrainz.org/) ReplayGain 2.0 plugin. Users that are not comfortable with command line interfaces may prefer this method since the plugin provides a GUI frontend to rsgain. See [MusicBrainz Picard Plugin](#musicbrainz-picard-plugin) for more information.

## Installation

Binary packages are available for some platforms on the [Release Page](https://github.com/complexlogic/rsgain/releases). You can also build the program yourself, see [BUILDING](docs/BUILDING.md).

### Windows

Download the ZIP file from the link below and extract its contents to a folder of your choice:
- [rsgain v3.4 portable ZIP (x64)](https://github.com/complexlogic/rsgain/releases/download/v3.4/rsgain-3.4-win64.zip)

rsgain should be run on Windows 10 or later for full compatibility, but it can run on Windows versions as early as Vista with some caveats. See [Windows Notes](#windows-notes) for more information.

It is recommended to add the directory to your `Path` system environment variable so you can invoke the program with the `rsgain` command instead of the path to its .exe file. 
1. Use Windows key + R to bring up the run box, then type `sysdm.cpl` and press enter
2. In the resulting window in the "Advanced" tab, click the "Environment variables" button. 
3. In the next window under "System variables", select "Path", then press "Edit".
4. Add the folder that you extracted `rsgain.exe` to.

#### Scoop

rsgain can also be installed from [several community Scoop buckets](https://scoop.sh/#/apps?q=rsgain&s=0&d=1&o=false). This installation method enables you to receive automatic upgrades to future versions, unlike the manual installation method described above.

### macOS

There is a Homebrew formula available for macOS users. Make sure you have the latest available Xcode installed, as well as Homebrew. Then, execute the following command:

```bash
brew install complexlogic/tap/rsgain
```

### FreeBSD

Available via ports tree or using packages (2023Q1 and later) as listed below:

```bash
cd /usr/ports/audio/rsgain && make install clean

pkg install rsgain
```

### Linux

#### Debian/Ubuntu

An amd64 .deb package is provided on the [release page](https://github.com/complexlogic/rsgain/releases/latest). It is installable on Debian Bookworm and Ubuntu 23.04.

```bash
wget https://github.com/complexlogic/rsgain/releases/download/v3.4/rsgain_3.4_amd64.deb
sudo apt install ./rsgain_3.4_amd64.deb
```

#### Arch/Manjaro

There is an AUR package [rsgain-git](https://aur.archlinux.org/packages/rsgain-git) based on the current `master` (which is relatively stable). You can install it with an AUR helper such as yay:

```bash
yay -S rsgain-git
```

There is also a PKGBUILD script based on the latest release source tarball located in the `config` directory of the repo.

#### Fedora

A package is available on the [release page](https://github.com/complexlogic/rsgain/releases/latest) that is compatible with Fedora 38.

```bash
sudo dnf install https://github.com/complexlogic/rsgain/releases/download/v3.4/rsgain-3.4-1.x86_64.rpm
```

#### Others

Users of other distros will need to build from source. See [BUILDING](docs/BUILDING.md).

### Docker

The repo contains a Dockerfile which can be used to run rsgain in a virtual environment. It will build a container based on the current Debian Stable release:

```bash
docker build -t rsgain https://github.com/complexlogic/rsgain.git
```

rsgain requires a relatively up-to-date operating system, so this method can be used to run rsgain on an older system if necessary.

A Docker container doesn't have access to the host filesystem by default. To use rsgain in a container, you need to mount your music library to a mount point in the container. Use the -v option followed by the path to your library and the mount point, separated by a colon. For example, if your music library is located at `/path/to/library`:

```bash
docker run -v /path/to/library:/mnt rsgain easy -m MAX /mnt
```

The docker log to `stdout` updates too slowly for the scan progress bar. If you don't use multithreaded mode consider passing `-q` to silence the output.

## Supported file formats

rsgain supports all popular file formats. See the below table for compatibility. rsgain sorts files internally based on file extension, so it is required that your audio files match one of the extensions in the second column of the table in order to be recognized as valid.

| Format                               | Supported File Extension(s) |
| ------------------------------------ | --------------------------- |
| Audio Interchange File Format (AIFF) | .aiff                       |
| Free Lossless Audio Codec (FLAC)     | .flac                       |
| Monkey's Audio                       | .ape                        |
| MPEG-1 Audio Layer II (MP2)          | .mp2                        |
| MPEG-1 Audio Layer III (MP3)         | .mp3                        |
| MPEG-4 Audio (AAC, ALAC)¹            | .m4a                        |
| Musepack (MPC)²                      | .mpc                        |
| Ogg (Vorbis, Speex, FLAC)            | .ogg, .oga, .spx            |
| Opus                                 | .opus                       |
| Tom's lossless Audio Kompressor      | .tak                        |
| Waveform Audio File Format (WAV)     | .wav                        |
| Wavpack                              | .wv                         |
| Windows Media Audio (WMA)            | .wma                        |

1. *Support for HE-AAC and xHE-AAC are available via the Fraunhofer AAC library. On Windows, the statically-linked FFmpeg already includes support, so no further action is required. On Unix platforms, you will need to check if your build of FFmpeg was compiled with the '--enable-libfdk-aac' option, and compile it yourself if necessary*
2. *Stream Version 8 (SV8) supported only. If you have files in the older SV7 format, you can convert them losslessly to SV8*

## Usage

rsgain contains two separate user interfaces: Easy Mode and Custom Mode. The distinction between the two modes is rooted in the history of ReplayGain utilities.

Legacy ReplayGain tagging utilities such as mp3gain did not support recursive directory-based scanning. The user was required to manually specify a list of files on the command line, preceded by options which were numerous and complex. This interface provided a lot of power and flexibility, but it wasn't particularly user friendly. Performing a full library scan typically required the user to supplement the program with a wrapper script that traversed the directory tree and detected the files.

rsgain's Easy Mode *is* that wrapper script; the functionality is built-in to the program. In Easy Mode, the user points the program to their library and it will be recursively scanned with all recommended settings enabled by default.

The legacy-style interface has been retained as "Custom Mode" for users that require a higher level of control. Custom Mode is mostly used for scripting.

### Easy Mode

Easy Mode recursively scans your entire music library using the recommended settings for each file type.
Easy Mode is invoked with the command `rsgain easy` followed by the root of the directory you want to scan:

```bash
rsgain easy /path/to/music/library
```

```powershell
rsgain easy "C:\path\to\music library"
```

Easy Mode assumes that you have you have your music library organized by album, so that each album is contained in its own folder. The album gain calculations rely on this assumption. If you do *not* have your music library organized by album, you should disable the album tags because the calculated values will not be valid. rsgain ships with a scan preset which can disable the album tags for you; invoke it with `-p no_album`. See the [Scan Presets](#scan-presets) section for more information about how the scan preset feature works.

#### Multithreaded Scanning

Easy Mode includes optional multithreaded operation to speed up the duration of a scan. Use the `-m` option, followed by the number of threads to create. The number of threads must not exceed the number that your CPU supports. For example, if you have a CPU with 4 threads:

```bash
rsgain easy -m 4 /path/to/music/library
```

If you don't know how many threads your CPU has, you can also specify `-m MAX` and rsgain will use the number provided by your operating system. This is useful for writing scripts where the hardware properties of the target machine are unknown.

Parallel scan jobs are generated on a *per-directory* basis, not a per-file basis. If you request 4 threads but there is only 1 directory to scan, a single thread will be working and the other 3 will sit idle the entire time. Multithreaded mode is optimized for scanning a very large number of directories. It is recommended to use multithreaded mode for full library scans and the default single threaded mode when incrementally adding 1 or 2 albums to your library.

The speed gains offered by multithreaded scanning are significant. With `-m 4` or higher, you can typically expect to see a 50-80% reduction in total scan time, depending on your hardware, settings, and library composition.

#### Skip Files with Existing Tags

rsgain has an option which will skip files with existing ReplayGain information, invoked by passing `-S` or `--skip-existing`. When enabled, rsgain will check whether the given file has a `REPLAYGAIN_TRACK_GAIN` tag, and skip scanning any files that do. If album tags are enabled, the files in the list will be judged collectively, i.e. if a single file is missing ReplayGain info, then *all* of them will be scanned.

This feature merely checks for the *existence* of the tags, and does not verify that the tags are complete, and are compatible with your current settings, e.g. target loudness. You should use this feature only if you are confident in the integrity of the files in the directory to be scanned. It's generally not a good idea to run this on files that you've recently download from the internet, which may have pre-existing ReplayGain information that was tagged by a different scanner.

#### Logging

You can use the `-O` option to enable scan logs. The program will save a tab-delimited file titled `replaygain.csv` with the scan results for every directory it scans. The log files can be viewed in a spreadsheet application.

##### Microsoft Excel

Microsoft Excel doesn't recognize the tab delimiter in CSV files by default. To enable Excel compatibility, rsgain has an option `-Os` which will add a `sep` header to the CSV file. This is a non-standard Microsoft extension which will enable the outputted CSV files to open in Excel.

##### Sorting

If you want the output sorted alphanumerically by filename, use the 'a' option, e.g. `-Oa`.

The options can be chained. For example, if you want both Excel compatibility and alphanumeric sorting, you can pass `-Oas`.

#### Scan Presets

Easy Mode scans files with the following settings by default:

- -18 LUFS target loudness
- Album tags enabled
- Sample peak calculations for peak tags
- Clipping protection enabled for positive gain values only (0 dB max peak)
- Standard uppercase tags for all formats
- Preserve the existing ID3v2 tag version for ID3 formats (e.g. MP3)
- Standard ReplayGain tags for Opus files

These settings are recommended for maximum compatibility with modern players. However, if you need one or more of the settings changed, you can use a preset file.

A preset file is an INI-formatted configuration file that contains sections enclosed in square brackets, and each section contains key=value pairs that correspond to settings. The first section in a presets file is titled "Global" and contains settings that will be applied to every format. The remaining sections pertain to a particular audio format, and the settings within them will only be applied to that format. This allows the user to define settings on a per-format basis. If a setting in the "Global" section is in conflict with one in the format-specific section, the format-specific value will always take precedence.

It should be noted that the format-specific configurations will only be applied if *all* files in the directory have the same file type. The Global settings will always be applied to files in directories that have multiple file types in them, regardless of the individual file type

A preset is specified with the `-p` option, followed by the path to a preset file *or* a preset name. A preset name is the filename of a preset without the directory or .ini file extension; rsgain will search the default preset locations for the file based on your platform:
- In the user's home directory (you will need to create this directory if it doesn't already exist):
  - Windows: `%USERPROFILE%\.rsgain\presets` (typically `C:\Users\<your username>\.rsgain\presets`)
  - macOS: `~/Library/rsgain/presets`
  - Linux: `~/.config/rsgain/presets`
- System location:
  - Windows: the folder `presets` in the same folder that contains `rsgain.exe`
  - macOS/Linux:`<install prefix>/share/rsgain/presets`

For example, rsgain ships with a preset `ebur128.ini`, which will scan files based on the EBU R 128 recommendations. You can invoke this preset with `-p ebur128`. rsgain also ships with a preset `default.ini`, which is pre-populated with all of the default settings. This preset is not intended to be used directly, but rather to serve as a base for users to create their own presets. It is not recommended for users to overwrite it. Instead, save a copy when using it as a base.

The settings in a preset file are applied in an "overrides" fashion. In other words, any settings or formats you're not interested in can be deleted from the preset and the defaults will be used instead.

Each setting key in a presets file corresponds to a command line option in Custom Mode. Below is a table of all settings available for use in a preset.

| Setting Key    | Value Type | Custom Mode Option |
| -------------- | ---------- | ------------------ |
| TagMode        | Character  | -s                 |
| TargetLoudness | Integer    | -l                 |
| AlbumGain      | Boolean    | -a                 |
| ClipMode       | Character  | -c                 |
| TruePeak       | Boolean    | -t                 |
| Lowercase      | Boolean    | -L                 |
| ID3v2Version   | Integer    | -I                 |
| MaxPeakLevel   | Decimal    | -m                 |
| OpusMode       | Character  | -o                 |

See [Custom Mode](#custom-mode) for more information.

#### Tag Mode `n`

Easy Mode features an exclusive fourth tag mod `n` which skips files. This is distinct from the `-S` command line option in that it will skip files regardless of whether or not they have ReplayGain information, and it can be applied on a per-format basis in a preset file.

This can be useful in the case that you only want to scan certain file formats. For example, suppose your Opus files are tagged with standard ReplayGain tags, but you later change your mind and want the R128_*_GAIN tags instead. Prepare a preset with the following:

```ini
[Global]
TagMode=n

[Opus]
TagMode=i
OpusMode=r
```

This will skip over all files *except* Opus, so you don't waste time scanning files that you don't want to change, as would otherwise be necessary with the `s` mode.

### Custom Mode

Custom Mode provides a more complex command line syntax that is similar in nature to mp3gain, loudgain, and other legacy ReplayGain scanners. Only the most basic settings are enabled by default. Unlike Easy Mode, Custom Mode works with files, not directories. Custom Mode is typically used for scripting.

Custom Mode is invoked with `rsgain custom` followed by options and a list of files to scan. For example, scan and tag a short list of MP3 files with album tags enabled:

```bash
rsgain custom -a -s i file1.mp3 file2.mp3 file3.mp3
```

Run `rsgain custom -h` for a full list of available options

### MusicBrainz Picard Plugin

[MusicBrainz Picard](https://picard.musicbrainz.org/) is a free, cross-platform music tagging application. Picard features a robust plugin ecosystem that greatly extends its functionality. rsgain serves as the backend for the ReplayGain 2.0 plugin, which is available from the official plugins repository. Users that prefer a graphical interface over a command line interface can use this plugin to scan their music library.

To install the plugin, navigate to the Options menu in Picard. Select "Plugins" in the sidebar, then find "ReplayGain 2.0" and click the download button. The plugin itself does not include rsgain; you'll still need to download and install rsgain separately per the [Installation](#installation) section for your chosen platform.

You need to set the path to rsgain in the plugin settings. This field is pre-populated with the `rsgain` command. On Unix platforms, programs are typically installed into a directory that's already in your `PATH`, so no further action is necessary in that case. On Windows, you will need to either manually add the folder containing rsgain to your `Path` as per the installation instructions, or use the exact path to `rsgain.exe` in the plugin settings.

To use the plugin, add files to Picard and associate them with a release (so the files are in the right window). The plugin can scan albums or individual tracks. Select one or more albums or tracks, then right click and select "Plugins->Calculate ReplayGain" from the context menu. This calculates the ReplayGain information for the selected items, but does not tag the files. The new tags are available for viewing in the metadata window at the bottom. Click the save button to write the new tags to file.

## Design Philosophy

This section provides a brief overview of modern audio theories, how they influenced the design of rsgain, and how rsgain differs from other popular ReplayGain scanners.

### What is Loudness?

Loudness can be defined as the *subjective* perception of sound pressure. The subjective nature of loudness presents challenges in prescribing normalization techniques.

#### Units

Sound is often measured in **decibels**, or dB for short. This is because there is an approximately logarithmic relationship between the sound pressure level and perceived loudness by the human ear.

Another unit used commonly in loudness normalization is the **loudness unit**, or LU for short. An LU is equivalent in magnitude to a dB. The reason that LU was introduced was provide context distinction; decibels can be used to measure many things, but with LU we are always referring to loudness.

The third common unit is **loudness units relative to full scale**, or LUFS for short. Full scale means the maximum sample value of a particular digital audio signal format. For example, a value of 32,767 is full scale in 16 bit signed integer audio. On a logarithmic scale, 0 dB/LU represents full scale, so loudness measurements that are referenced to it (LUFS) will always be negative. LUFS is the most common unit for measuring audio loudness today.

### Normalizing Loudness

Early attempts at loudness normalization measured a recording based on its maximum sample value, known as the **peak**. This approach was flawed because recordings vary significantly in dynamic range. Dynamic range is the difference between the high and the low parts of an audio signal. For example, consider a song that is somewhat quiet, but has a single, very loud snare drum hit. This snare drum hit, while being very loud for a short period of time, is not representative of the overall loudness of the song due to its large dynamic range.

Later attempts measured signal loudness using an averaging technique known as root mean square, or RMS for short. This proved to be a much more effective measure of signal loudness than the peak.

Another advancement was made in the weighting of frequencies. The human ear is not equally sensitive to all frequencies. A mid frequency is perceived to be louder than a low or high frequency of equivalent sound pressure level. The relationship between frequency and perceived loudness is often referred to as the **Fletcher-Munson curve** or the **equal-loudness contour**.

The original ReplayGain specification from 2001 combined the RMS averaging with a frequency weighting filter that compensated for the Fletcher-Munson curve. Since that time, a new industry standard [ITU-R BS.1770](https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-4-201510-I!!PDF-E.pdf) has been published. It details a new loudness measurement algorithm that implements RMS averaging and frequency weighting, similar in nature to ReplayGain 1.0, but far less computationally intensive and shown in listening tests to be more accurate. This new algorithm provides the basis for the ReplayGain 2.0 specification upon which rsgain is based.

#### Target Loudness

The loudness measurement algorithm specified in BS.1770 has gained widespread adoption in loudness normalization since its publishing. However, one aspect that is not well agreed upon is the target loudness level that audio signals should be normalized to. Many people perceive louder audio signals to be of higher quality, so the systems that target casual listeners generally tend to opt for higher target loudness levels. Higher target loudness levels are more prone to clipping, an unwanted form of distortion, so systems that target a more serious audience generally tend to opt for lower target loudness. By default, rsgain uses the -18 LUFS target loudness value specified by ReplayGain, but users have the ability to change it with the `-l` option.

The table below gives a brief summary of the target loudness levels used by various organizations to demonstrate the range of typical values.

| Target Loudness | Adopters                                   |
| --------------- | ------------------------------------------ |
| -14 LUFS        | Spotify, YouTube Music, Amazon Prime Music |
| -16 LUFS        | Apple Music                                |
| -18 LUFS        | ReplayGain 2.0                             |
| -23 LUFS        | European Broadcasting Union (EBU)          |

### Sample Peak vs. True Peak

The ReplayGain specification requires a scanner to tag files with peak information, which is intended for use in predicting whether an audio signal will clip. There are two common ways to calculate this value: sample peak and true peak.

The sample peak is the highest value sample in the signal. In ReplayGain, the peak is unitless and normalized to a scale of 0 to 1, with 1 representing digital full scale. The sample peak has been the default peak measurement used in loudness normalization until recently.

True peak is a relatively new concept. The theory pertains to how audio signals are converted from analog to digital, and then eventually from digital back to analog for listening. In the first stage (analog to digital), the sampling process does not capture the true peak of the continuous analog signal because it occurred in between samples. The sample peak value that is calculated using the digital samples is therefore inaccurate. The digital signal is then mastered with the deceptively low sample peak set to just below full scale. When it's converted back into analog during playback, the true peak from the original analog signal is reconstructed and exceeds full scale, resulting in clipping.

In the case of a digital audio recording, true peak from the original analog signal has already been lost forever. The method used to calculate the "true" peak from an existing digital audio signal is called interpolation, which attempts to *approximate* the original analog signal. The digital signal is resampled at a much higher sampling rate, and the interpolation algorithm attempts to estimate what original analog signal was in between the original samples. The higher oversampling rate, the better approximation of the original analog signal. Peaks that occur in the interpolated samples are known as intersample peaks. In practice, the calculated true peak value will always be higher than the sample peak. Unlike sample peaks, the true peak can exceed full scale (1).

The ReplayGain specification does not explicitly specify whether the peak should be calculated using the sample peak or true peak method, leaving the decision to the implementation. Comparing popular ReplayGain scanners, r128gain always uses sample peak, while loudgain always uses true peak. Conversely, rsgain allows the user to choose between the sample peak and true peak methods. The default is sample peak.

Using true peak instead of sample peak comes at a significant performance cost. Scans using true peak will typically be 2-4x longer than otherwise equivalent sample peak scans; the oversampling interpolation process used to calculate the true peak is very computationally intensive.

### Clipping Protection

Clipping is a form of distortion that occurs when a signal exceeds its maximum bound, and is generally considered undesirable for audio playback. The ReplayGain standard requires scanners to tag files with the peak information as a means of preventing clipping during playback.

However, not all ReplayGain-compatible players actually implement clipping protection using the peak tags. rsgain has a clipping protection feature that attempts to prevent clipping at *scan-time* instead of during playback. It is specified with the `-c` option, with a required character argument:

- `n`: No clipping protection (default in Custom Mode)
- `p`: Clipping protection for positive gain values only (default in Easy Mode)
- `a`: Always clip protect

The clipping protection works by adjusting the calculated peak by the calculated gain value (as it would be during playback). If this "new" peak value exceeds the maximum peak (full scale by default), then the gain will be adjusted lower by the excess amount, bringing the "new" peak down to the maximum level.

Adjusting the gain has the side effect of lowering the loudness of the song below the target level. If this occurs in a significant number of files, it results in an uneven distribution of loudness across your music library, which defeats the purpose of applying ReplayGain in the first place. Easy Mode outputs a useful statistic "Clip Adjustments" at the end of every scan which can help you gauge how often the clipping protection is kicking in for your current settings.

Scan-time clipping protection mechanisms have drawbacks and limitations. In general clipping protection can be implemented much more effectively by the player than by the scanner. If you know your player reads the peak tags and supports clipping protection, consider disabling rsgain's clipping protection entirely. Another option is to lower your target loudness level, which will signficantly reduce the number of files which activate the clipping protection.

### Opus Files

Opus files are governed by [RFC 7845](https://datatracker.ietf.org/doc/html/rfc7845), which introduced a competing loudness normalization method that is completely incompatible with ReplayGain:

- The gain tags are `R128_TRACK_GAIN` and `R128_ALBUM_GAIN` instead of `REPLAYGAIN_TRACK_GAIN` and `REPLAYGAIN_ALBUM_GAIN`
- Peak tags are not supported
- The gain values are stored in a Q7.8 fixed point integer string, instead of the standard base-10 decimal string
- The gains are referenced to -23 LUFS instead of -18 LUFS

Additionally, there is also an "output gain" field in the header, which contains another volume adjustment that needs to be taken into account.

To handle the complexity, rsgain has a Opus Mode setting with a 5 choice character option that determines how Opus files should be tagged:

- `d`: Write standard ReplayGain tags, set header output gain to 0
- `r`: Write R128_*_GAIN tags, set header output gain to 0
- `s`: Same as 'r' above, plus the target loudness is forced to -23 LUFS for Opus files only
- `t`: Write track gain to header output gain
- `a`: Write album gain to header output gain

Since rsgain is a *ReplayGain* scanner, the `d` mode is the default, even though the ReplayGain standard conflicts with RFC 7845. In my opinion, the authors of RFC 7845 totally overstepped their authority by specifying a format-specific loudness normalization method. Particularly egregious is the specification of a target loudness level. There is no one-size-fits-all solution for target loudness. The best value depends on the dynamic range of your music, which tends to vary by genre. Moreover, most people do not have a music library comprised entirely of a single audio format, so format-specific loudness normalization methods are inappropriate. Having Opus files play back 5 dB quieter than all other file types defeats the purpose of applying ReplayGain.

Some players will automatically add a +5 dB pregain to Opus files to attempt to compensate for the difference in target loudness between the RFC 7845 normalization method and ReplayGain. foobar2000 and a few others are among those that do this. You'll need to research how your chosen player(s) handle Opus files, and adjust your settings in rsgain accordingly. 

If you wish to write tags that are fully compliant to RFC 7845 instead of ReplayGain 2.0, you can use the `-o` 's' option. For example, as an Easy Mode preset

```ini
[Opus]
OpusMode=s
```

### Tag casing

There is much to be said about uppercase versus lowercase tags. In my experience, the vast majority of *modern* players recognize the standard uppercase tags for the vast majority of file formats. rsgain will write the standard uppercase tags by default for all formats.

If you do encounter a player that doesn't recognize the uppercase tags, my advice is this: inform the developers with an issue - or even submit a PR yourself - rather than contribute to the fragmentation of the ReplayGain ecosystem by legitimizing the lowercase tags. Use the `-L` lowercase tags option only when all other options have been exhausted.

## Windows Notes

rsgain uses UTF-8 for Unicode, while Windows has historically used a subset of UTF-16. However, Microsoft added full UTF-8 support in [Windows 10 version 1903](https://docs.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page). Therefore, it is strongly recommended to run rsgain on Windows 10 or later to ensure compatibility with all filenames. You can still run rsgain on Windows Vista through 8.1 if you don't need Unicode support, i.e. you don't have any filenames with non-ANSI characters.

Another caveat with Windows is the performance of the scan progress bar. Console output on Windows is notoriously slow compared to Unix platforms. Unfortunately, the progress bar can be a bottleneck, particularly with the default sample peak calculations. I have decided to leave it as-is, under the rationale that the performance in the single-threaded mode is less important than in [multithreaded](#multithreaded-scanning), which is unaffected by this issue since the progress bar is disabled. If you do need to tag a large number of files using the single-threaded Easy Mode or Custom Mode, pass the `-q` option to disable the progress bar, which will eliminate the bottleneck.

## License

rsgain is a very heavily modified fork of [loudgain](https://github.com/Moonbase59/loudgain), and is licensed accordingly under the original 2 clause BSD license used by loudgain.
