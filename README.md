# rsgain
**rsgain** (**r**eally **s**imple **gain**) is a ReplayGain 2.0 tagging utility for Windows, macOS, and Linux. rsgain applies loudness metadata tags to your files, while leaving the audio stream untouched. A ReplayGain-compatible player will dynamically adjust the volume of your tagged files during playback.

rsgain is a very heavily modified fork of [loudgain](https://github.com/Moonbase59/loudgain). The goal of rsgain is to take the excellent platform created by loudgain and simplify it for the average user, while also preserving the advanced features for users that need them. The following improvements have been made from loudgain:
- Native Windows support
- Built-in recursive directory scanning without the need for a wrapper script. See [Easy Mode](#easy-mode) for more details.
- Multithreaded scanning

## Installation
Binary packages are available for some platforms on the [Release Page](https://github.com/complexlogic/rsgain/releases). You can also build the program youself, see [BUILDING](docs/BUILDING.md).

### Windows
rsgain is compatible with Windows 10 and later. Download the win64 .zip file from the [latest release](https://github.com/complexlogic/rsgain/releases/latest) and extract its contents to a directory of your choice. 

It is recommended to add the directory to your `Path` system environment variable so you can invoke the program with the `rsgain` command instead of the path to its .exe file. In the Windows taskbar search, type "env", then select "Edit the system environment variables". In the resulting window, click the "Environment variables" button. In the next window under "System variables", select "Path", then press Edit. Add the folder that you extracted `rsgain.exe` to in the previous step.

### macOS
There is currently no binary package available for macOS, so Mac users will need to build from source. See [BUILDING](docs/BUILDING.md).

If anybody is willing to maintain a Homebrew package for rsgain, please reach out by making an issue on the  [Issue Tracker](https://github.com/complexlogic/rsgain/issues).

### Linux
#### Debian/Ubuntu
An amd64 .deb package is provided on the [release page](https://github.com/complexlogic/rsgain/releases/latest). It is installable on Debian Bullseye and later, Ubuntu 21.04 and later.

#### Arch/Manjaro
There is an AUR package [rsgain-git](https://aur.archlinux.org/packages/rsgain-git) based on the current `master` (which is relatively stable). You can easily install it with an AUR helper such as yay:
```
yay -S rsgain-git
```
Alternatively, this repo also hosts a PKGBUILD script based on the latest release source code tarball. To install, run the following commands from a clean working directory:
```
wget https://raw.githubusercontent.com/complexlogic/rsgain/master/config/PKGBUILD
makepkg -si
```

#### Others
Users of other distros will need to build from source. See [BUILDING](docs/BUILDING.md).

## Supported file formats
rsgain supports all popular file formats (as well as a few not-so-popular ones). See the below table for compatibility. It should be noted that rsgain sorts files internally based on file extension, so it is required that your audio files match the second column in the table in order to be recognized as valid.
|Format | Supported File Extension(s) |
|-------|-----------------------------|
|MP3|.mp3|
|FLAC|.flac|
|Ogg (Vorbis, Speex, FLAC)|.ogg, .oga, .spx|
|Opus|.opus|
|MPEG-4 Audio (AAC, ALAC)|.m4a|
|Wavpack|.wv|
|Monkey's Audio|.ape|
|WMA|.wma|
|MP2|.mp2|
|WAV|.wav|
|AIFF|.aiff|

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
rsgain easy "C:\path\to\music library"
```
That's it. rsgain will take care of the details. See [Overrides](#overriding-default-settings) for more information about the default settings and how to override them, if desired.

#### Multithreaded Scanning
Easy Mode includes optional multithreaded operation to speed up the duration of a scan. Use the `-m` option, followed by the number of threads to create. The number of threads must not be more than the number that your CPU supports. For example, if you have a CPU with 4 threads:
```
rsgain easy -m 4 /path/to/music/library
``` 
If you don't know how many threads your CPU has, you can also specify `-m MAX` and rsgain will use the number provided by your operating system. This is useful for writing scripts where the hardware properties of the target machine are unknown.

Parallel scan jobs are generated on a *per-directory* basis, not a per-file basis. If you request 4 threads but there is only 1 directory to scan, a single thread will be working and the other 3 will sit idle the entire time. Multithreaded mode is optimized for scanning a very large number of directories. It is recommended to use multithreaded mode for full library scans and the default single threaded mode when incrementally adding 1 or 2 albums to your library.

The speed gains offered by multithreaded scanning are significant. On my library of about 12,000 songs, it takes about 5 hours in the default single threaded mode, but only 1 hour 45 minutes hours with `-m 4`, a reduction of about 65%.

#### Logging
You can use the `-O` option to enable scan logs. The program will save a tab-delimited file titled `replaygain.csv` with the scan results for every directory it scans. The log files can be viewed in a spreadsheet application such as Microsoft Excel or LibreOffice Calc.

#### Overriding Default Settings
Easy Mode scans files with the following settings by default:
- -18 LUFS target loudness
- Album tags enabled
- True peak calculations for peak tags
- Clipping protection enabled for positive gain values only (-1 dB max peak)
- Standard uppercase tags for all formats
- ID3v2.3 tags for ID3 formats
- Standard ReplayGain tags for Opus files

These settings are recommended for maximum compatibility with modern players. However, if you need one of the settings changed, you can override them on a per-format basis using the `-o` option, and an overrides file.

The overrides file is an INI-formatted file that contains sections enclosed in square brackets which correspond to the available formats, and each section contains key=value pairs that correspond to settings. The overrides feature is intended for users that can't use the default settings of Easy Mode, but still prefer the functionality of Easy Mode over Custom Mode.

For example, Easy Mode writes ID3v2.3 tags on MP3 files by default, but suppose you want ID3v2.4 instead. Format your `overrides.ini` file as follows:
```INI
[MP3]
ID3v2Version=4
```
Then, pass the path to the overrides file with the `-o` option:

```
rsgain easy -o /path/to/overrides.ini /path/to/music/library
```
A default `overrides.ini` file ships with rsgain in the root package directory for Windows, and in `<install prefix>/share/rsgain` for Unix. The file is pre-populated with all settings that are available to change. Note that this is an *overrides* file, not a configuration file, i.e any formats or settings you're not interested in can simply be deleted and the defaults will be used instead.

Each setting key corresponds to a command line option in Custom Mode. Below is a table of all settings available for override.

|Setting Key | Value Type | Custom Mode Option|
|------------|------------|-------------------|
|TagMode|Character|-s|
|TargetLoudness|Integer|-l|
|AlbumGain|Boolean|-a|
|ClipMode|Character|-c|
|TruePeak|Boolean|-t|
|Lowercase|Boolean|-L|
|ID3v2Version|Integer|-I|
|MaxPeakLevel|Decimal|-K|
|R128Tags|Boolean|-o|

See the [Custom Mode help](#command-line-help) for more information.

### Custom Mode
Custom Mode provides more complex command line syntax that is similar in nature to mp3gain and loudgain. Only the most basic settings are enabled by default. Unlike Easy Mode, Custom Mode works with files, not directories. If you want recursive directory-based scanning, you will need to write a wrapper script.

Custom Mode is invoked with `rsgain custom` followed by options and a list of files to scan. For example, scan and tag a short list of MP3 files with album tags enabled:
 ```
 rsgain custom -a -s i file1.mp3 file2.mp3 file3.mp3
 ```
Run `rsgain custom -h` for a full list of available options


## Design Philosophy 
This section contains a brief overview of modern audio theories, how they influenced the design of rsgain, and how rsgain differs from other popular ReplayGain scanners.

### What is Loudness?
Loudness can be defined as the *subjective* perception of sound pressure. The subjective nature of loudness presents challenges in prescribing normalization techniques.

#### Units
Sound is often measured in **decibels**, or dB for short. This is because there is an approximately logarithmic relationship between the sound pressure level and perceived loudness by the human ear.

Another unit used commonly in loudness normalization is the **loudness unit**, or LU for short. An LU is equivalent to a dB. The reason that LU was introduced was provide context distinction; decibels can be used to measure many things, but with LU we are always referring to loudness.

The third common unit is **loudness units relative to full scale**, or LUFS for short. Full scale means the maximum sample value of a particular digital audio signal format. For example, a value of 32,767 is full scale in 16 bit signed integer audio. On a logarithmic scale, 0 dB/LU represents full scale, so loudness measurements that are referenced to it (LUFS) will always be negative. LUFS is the most common unit for measuring audio loudness today.

### Normalizing Loudness
Early attempts at loudness normalization measured a recording based on its maximum sample value, known as the **peak**. This approach was flawed because recordings vary significantly in dynamic range. Dynamic range is the difference between the high and the low parts of an audio signal. For example, consider a song that is somewhat quiet, but has a single, very loud snare drum hit. This snare drum hit, while being very loud for a short period of time, is not representative of the overall loudness of the song due to its large dynamic range.

Later attempts measured signal loudness using an averaging technique known as root mean square, or RMS for short. This proved to be a much more effective measure of signal loudness than the peak. 

Another advancement was made in the weighting of frequencies. The human ear is not equally sensitive to all frequencies. A mid frequency is perceived to be louder than a low or high frequency of equivalent sound pressure level. The relationship between frequency and perceived loudness is often referred to as the **Fletcher-Munson curve** or the **equal-loudness contour**.

The original ReplayGain specification from 2001 combined the RMS averaging with a frequency weighting filter that compensated for the Fletcher-Munson curve. Since that time, a new industry standard [ITU-R BS.1770](https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-4-201510-I!!PDF-E.pdf) has been published. It details a new loudness measurement algorithm that implements RMS averaging and frequency weighting, similar in nature to ReplayGain 1.0, but far less computationally intensive and proven in listening tests to be more accurate. This new algorithm provides the basis for the ReplayGain 2.0 specification upon which rsgain is based.

#### Target Loudness
The loudness measurement algorithm specified in BS.1770 has gained widespread adoption in loudness normalization since its publishing. However, one aspect that is not well-agreed upon is the target loudness level that audio signals should be normalized to. Many people perceive louder audio signals to be of higher quality, so the systems that target casual listeners generally tend to opt for higher target loudness levels. Higher target loudness levels are more prone to clipping, an unwanted form of distortion, so systems that target a more serious audience generally tend to opt for lower target loudness. By default, rsgain uses the -18 LUFS target loudness value specified by ReplayGain, but users have the ability to change it with the `-l` option.

The table below gives a brief summary of the target loudness levels used by various organizations to demonstrate the range of typical values.

|Target Loudness | Adopters |
|------------|------------|
|-14 LUFS|Spotify, YouTube Music, Amazon Prime Music|
|-16 LUFS|Apple Music|
|-18 LUFS|ReplayGain|
|-23 LUFS|European Broadcast Union (EBU)|

### Sample Peak vs. True Peak
The ReplayGain specification requires a scanner to tag files with peak information, which is intended for use in predicting whether an audio signal will clip. There are two common ways to calculate this value: sample peak and true peak.

The sample peak is simply the highest value sample in the signal. In ReplayGain, the peak is unitless and normalized to a scale of 0 to 1, with 1 representing full scale. The sample peak has been the default peak measurement used in loudness normalization until recently.

True peak is a relatively new concept. The theory pertains to how audio signals are converted from analog to digital, and then eventually from digital back to analog for listening. In the first stage (analog to digital), the sampling process does not capture the true peak of the continuous analog signal because it occurred in between samples. The sample peak value that is calculated using the digital samples is therefore inaccurate because the true peak was lost in the sampling process. The digital signal is mastered with the deceptively low sample peak set to just below full scale. When it's converted back into analog during playback, the true peak from the original analog signal is reconstructed and exceeds full scale, resulting in clipping.

In the case of a digital audio recording, true peak from the original analog signal has already been lost forever. The method used to calculate the "true" peak from an existing digital audio signal is called interpolation, which attempts to *approximate* the original analog signal. The digital signal is resampled at a much higher sampling rate, and the interpolation algorithm attempts to estimate what original analog signal was in between the original samples. The higher oversampling rate, the better approximation of the original analog signal. Peaks that occur in the interpolated samples are known as intersample peaks. In practice, the calculated true peak value will always be higher than the sample peak. Unlike sample peaks, the true peak can exceed full scale (1).

The ReplayGain specification does not specify whether the peak should be calculated using the sample peak or true peak method, leaving the decision to the implementation. Comparing popular ReplayGain scanners, r128gain always uses sample peak, while loudgain always uses true peak. Conversely, rsgain allows the user to choose between the sample peak and true peak methods. The default in Easy Mode is true peak.

It should be noted that using true peak instead of sample peak comes at a significant performance cost. Scans using true peak will typically be 2-4x longer than otherwise equivalent sample peak scans; the oversampling interpolation process used to calculate the true peak is very computationally intensive.

### Clipping Protection

### Opus Files

### Tag casing


