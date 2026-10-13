#!/bin/bash
# Compiles the pure-logic modules (settings, stats, insights, i18n) against the
# stub pebble.h in this directory and runs the assertions. Covers the parts that
# cannot be eyeballed in the emulator: score maths, coaching rule order, date
# arithmetic behind the streak, the history ring and settings persistence.
set -euo pipefail
cd "$(dirname "$0")"

# Built as the Pebble Time 2 (emery), the one watch with a heart-rate sensor;
# HAS_HRM=0 is covered by a second build of the same tests below.
gcc -std=gnu11 -Wall -Wextra -Wno-unused-parameter -I. -DPBL_PLATFORM_EMERY \
  test_logic.c stubs.c \
  ../../src/c/settings.c ../../src/c/stats.c ../../src/c/insights.c ../../src/c/i18n.c \
  -lm -o /tmp/cairn_test

/tmp/cairn_test

echo "--- without a heart-rate sensor (gabbro, flint)"
gcc -std=gnu11 -Wall -Wextra -Wno-unused-parameter -I. \
  test_logic.c stubs.c \
  ../../src/c/settings.c ../../src/c/stats.c ../../src/c/insights.c ../../src/c/i18n.c \
  -lm -o /tmp/cairn_test_nohrm

/tmp/cairn_test_nohrm
