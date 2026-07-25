#!/bin/bash
# Renders the generated cues (sfx_synth.c) to WAV files you can play on a PC.
# Output: build/sfx-preview/*.wav — one per cue, plus peak/RMS numbers so a
# silent or permanently clipping cue is obvious without listening.
set -euo pipefail
cd "$(dirname "$0")"
OUT="../../build/sfx-preview"
mkdir -p "$OUT"

gcc -std=gnu11 -Wall -Wextra -Wno-unused-parameter -I. \
  preview_sfx.c -lm -o /tmp/cairn_preview_sfx

/tmp/cairn_preview_sfx "$OUT"
