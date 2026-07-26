# Phase-tone test protocol

## What this is

The breathing phases in Cairn are haptic only. Marking them with the speaker as
well froze the app and restarted PebbleOS on real hardware — twice, as streamed
PCM and again as plain notes, once leaving the watch in factory mode. See
`src/c/sfx.c` and the `CHANGELOG.md` entry.

The diagnosis on record (commit `5f10be9`) is that **`speaker_play_notes()`
called from an `AppTimer` callback takes the OS down**. Every speaker call that
still works is made from `init()` (`src/c/cairn.c`), a click handler
(`src/c/settings_window.c`) or a window `.appear` handler
(`src/c/result_window.c`). The phase cues are the only ones fired from a timer —
the 40 ms phase machine in `src/c/session.c`.

But that was never isolated from a second variable: the phase cues were also the
**longest** things being queued — five notes, ~450 ms — while the surviving
calls are short. This test separates the two.

## The hypothesis under test

`CAIRN_PHASE_TONES=1` now plays **one 70 ms note per phase** instead of the
five-note swell (`SFX_IN_1` / `SFX_HOLD_1` / `SFX_OUT_1` in `src/c/sfx.c`).

| Outcome | Reading |
|---|---|
| The session runs clean | The **phrase length** was the problem. A short cue from a timer is survivable, and a re-tuned single-note phase cue is worth designing. |
| It still freezes / restarts | The **timer callback context** is the problem. Length is irrelevant; the fix is to get the call out of the timer entirely (set a flag in the callback, play from the layer's update proc or a click handler). |

Either answer closes the question. There is no third reading, which is why it is
worth the risk of one run.

## Before you start

- **Use a watch you can afford to brick.** One of these experiments already put
  a watch into factory mode. This is not a formality.
- Note down anything on the watch you would not want to lose.
- The emulator **cannot** run this test. QEMU has no speaker and the app exits on
  the first note; a `CAIRN_MUTE=1` build would compile the note away and prove
  nothing. Hardware only.

## Build and install

The flag is read in `configure(ctx)` in `wscript`, so waf caches it — the clean
is not optional:

```bash
cd ~/Development/pebble/Cairn
pebble clean
CAIRN_PHASE_TONES=1 pebble build
pebble install --cloudpebble        # via Dev Connect
```

Confirm you have the right build before trusting the result: open Settings and
cycle Sound. The sample blip there plays from a click handler and is unchanged,
so it tells you the speaker works but says nothing about the experiment.

## The run

Settings to use, chosen to reach the failure point fast:

| Setting | Value | Why |
|---|---|---|
| Rhythm | **Box** (4-4-4-4) | Four phase transitions per 16 s cycle — the densest cue rate of the presets, and the only one that exercises in, hold **and** out. |
| Length | **1 minute** | ~15 cues. Past failures hit at the second or third cue, so a minute is several times the margin needed. |
| Sound | **Soft** | The level the app ships with. Do not test at Loud first; if amplitude turns out to matter, you want the shipping level to be the one you have data on. |
| Haptics | **Off** | Removes vibration as a confound. The earlier hardware freeze was reproduced with vibration off, so this matches the conditions on record. |
| Backlight | **Always** | A screen that stays lit makes a freeze obvious the moment it happens. |

Then: start a session, **hold the watch in view for the whole minute**, and don't
touch the buttons.

## What to write down

1. **How far it got** — the cue number it reached, or "finished the minute". The
   phase counter on screen is enough; you don't need the exact tick.
2. **What failure looked like**, if any:
   - app exits to the watchface (an app fault — different from the past failure),
   - screen freezes and stays frozen (the watchdog is about to fire),
   - watch reboots / shows the PebbleOS boot screen (the past failure),
   - Bluetooth connection drops without the screen changing (also the past
     failure — the logs showed no app fault, just the link dying).
3. **Whether the notes were actually audible.** A silent run that survives proves
   nothing: it could be that Quiet Time was on, or the level rounded to silence.
   If you did not hear the notes, the run does not count.
4. **How you recovered**, if it went down.

Do **not** run `pebble logs` — it blocks forever, even under `timeout`. That is
also why the original diagnosis had to be made from the connection dropping
rather than from an app fault message.

## Afterwards

Whatever the outcome, **rebuild without the flag before using the app normally**:

```bash
pebble clean && pebble build && pebble install --cloudpebble
```

`CAIRN_PHASE_TONES` is a diagnostic, not a feature. A single note per phase has
none of the rise-and-fall shape the breathing cue needs, so even a clean run does
not make this build shippable — it makes a *designed* short cue worth trying.
