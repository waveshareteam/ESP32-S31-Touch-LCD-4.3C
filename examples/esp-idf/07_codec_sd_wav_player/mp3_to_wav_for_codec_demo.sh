#!/usr/bin/env bash
set -euo pipefail

show_help() {
    cat <<'EOF'
Convert MP3 to WAV for the ESP32 codec SD WAV player demo.

The generated WAV format is compatible with:
  demo/07_codec_sd_wav_player/main/main.c

Default output:
  test.wav

Default format:
  PCM signed 16-bit little-endian, stereo, 44.1 kHz

Usage:
  ./mp3_to_wav_for_codec_demo.sh input.mp3 [output.wav] [sample_rate] [channels]

Examples:
  ./mp3_to_wav_for_codec_demo.sh music.mp3
  ./mp3_to_wav_for_codec_demo.sh music.mp3 /media/$USER/SDCARD/test.wav
  ./mp3_to_wav_for_codec_demo.sh music.mp3 test.wav 16000 1

Install ffmpeg:
  Ubuntu/Debian: sudo apt install ffmpeg
  Fedora:        sudo dnf install ffmpeg
  Arch:          sudo pacman -S ffmpeg
  macOS:         brew install ffmpeg
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    show_help
    exit 0
fi

if [[ $# -lt 1 ]]; then
    show_help
    exit 1
fi

if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "Error: ffmpeg was not found." >&2
    echo "Install it first, for example: sudo apt install ffmpeg" >&2
    exit 1
fi

input_mp3="$1"
output_wav="${2:-test.wav}"
sample_rate="${3:-44100}"
channels="${4:-2}"

if [[ ! -f "$input_mp3" ]]; then
    echo "Error: input file does not exist: $input_mp3" >&2
    exit 1
fi

if [[ "$channels" != "1" && "$channels" != "2" ]]; then
    echo "Error: channels must be 1 or 2." >&2
    exit 1
fi

mkdir -p "$(dirname "$(realpath -m "$output_wav")")"

echo "Input : $input_mp3"
echo "Output: $output_wav"
echo "Format: PCM s16le, ${sample_rate} Hz, ${channels} channel(s)"

ffmpeg \
    -y \
    -i "$input_mp3" \
    -vn \
    -map_metadata -1 \
    -ac "$channels" \
    -ar "$sample_rate" \
    -sample_fmt s16 \
    -c:a pcm_s16le \
    -f wav \
    "$output_wav"

if command -v ffprobe >/dev/null 2>&1; then
    codec_name="$(ffprobe -v error -select_streams a:0 -show_entries stream=codec_name -of default=noprint_wrappers=1:nokey=1 "$output_wav")"
    bits_per_sample="$(ffprobe -v error -select_streams a:0 -show_entries stream=bits_per_sample -of default=noprint_wrappers=1:nokey=1 "$output_wav")"
    out_channels="$(ffprobe -v error -select_streams a:0 -show_entries stream=channels -of default=noprint_wrappers=1:nokey=1 "$output_wav")"
    out_rate="$(ffprobe -v error -select_streams a:0 -show_entries stream=sample_rate -of default=noprint_wrappers=1:nokey=1 "$output_wav")"

    if [[ "$codec_name" != "pcm_s16le" || "$bits_per_sample" != "16" ]]; then
        echo "Error: output WAV verification failed: codec=$codec_name bits=$bits_per_sample" >&2
        exit 1
    fi

    echo "Verified WAV: PCM 16-bit, ${out_rate} Hz, ${out_channels} channel(s)"
fi

echo
echo "Done. Put this file in the SD card root directory as test.wav:"
echo "$output_wav"
