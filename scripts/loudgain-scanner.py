#!/usr/bin/env python3
import os
import sys
import shutil
import glob
import subprocess
import platform

if platform.system() == "Windows":
    EXECUTABLE_TITLE = "loudgain.exe"
else:
    EXECUTABLE_TITLE = "loudgain"

SCAN_ARGS = {
    '.flac': ['custom', '-a', '-k', '-s', 'e'],
    '.ogg':  ['custom', '-a', '-k', '-s', 'e'],
    '.mp3':  ['custom', '-I3', '-S', '-L', '-a', '-k', '-s', 'e'],
    '.m4a':  ['custom', '-L', '-a', '-k', '-s', 'e'],
    '.opus': ['custom', '-a', '-k', '-s', 'e'],
    '.wma':  ['custom', '-L', '-a', '-k', '-s', 'e'],
    '.wav':  ['custom', '-I3', '-L', '-a', '-k', '-s', 'e'],
    '.aiff': ['custom', '-I3', '-L', '-a', '-k', '-s', 'e'],
    '.wv':   ['custom', '-S', '-a', '-k', '-s', 'e'],
    '.ape':  ['custom', '-S', '-a', '-k', '-s', 'e']
}

def scan(directory):
    for root, subdirs, files in os.walk(directory):
        if len(files) == 0:
            print(f"No files found in directory {root}")
        else:
            extensions = list()
            for file in files:
                extension = os.path.splitext(file)[1]
                if extension in SCAN_ARGS and extension not in extensions:
                    extensions.append(extension)
            length = len(extensions)
            if length == 0:
                print(f"No audio files found in directory {root}")
            elif length > 1:
                print(f"Multiple audio file types detected in directory {root}, skipping")
            else:
                album_extension = extensions[0]
                os.chdir(root)
                audio_files = glob.glob("*" + album_extension)
                command = [loudgain] + SCAN_ARGS[album_extension] + audio_files
                subprocess.run(command)

if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.realpath(__file__))
    executable_path = os.path.join(script_dir, EXECUTABLE_TITLE)
    if os.path.isfile(executable_path):
        loudgain = executable_path
    elif shutil.which("loudgain") is not None:
        loudgain = "loudgain"
    else:
        print("Error: loudgain not found. Make sure it's in your PATH or the same directory as the script")
        sys.exit(1)
    if (len(sys.argv)) < 2:
        print("Error: You must pass the path of the root directory to scan as the first argument")
        sys.exit(1)
    if os.path.isdir(sys.argv[1]) is False:
        print(f"Error: Directory {sys.argv[1]} does not exist")
        sys.exit(1)
    scan(sys.argv[1])
    sys.exit(0)
