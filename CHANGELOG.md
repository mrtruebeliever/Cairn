# Changelog

## 1.3.0

- **A button bar instead of the hint line.** The home screen's cycling hint
  line is gone; a bar along the buttons shows what each one does, the short
  press on top (up, start, down) and the long press beneath it in grey
  (history, progress, settings). On the round screen the icons ride the bezel.
- **Settings follow the watch's text size.** The rows are Gothic 24 at the
  watch's default size on the 228px screens (18 on the 144px one) and step one size
  down or up with Settings → Display → Text size; the value column is measured
  instead of fixed, and arrows show when the list scrolls.
- **A singing bowl.** Opening the app, finishing a session, earning a badge
  and the reminder are now a struck singing bowl (a generated sample, see
  `tools/gen_sfx.py`) instead of bright sine chimes: lower and longer at the
  end of a session, higher with a small sparkle for a badge. The Pebble 2 Duo,
  short on memory, keeps the chimes.
- **Daily reminder.** Settings → Reminder (or the phone settings) picks a time,
  Off by default. The watch then opens Cairn, vibrates and strikes the bowl —
  unless a session was already done that day or Quiet Time is on, in which
  case it stays silent. The settings list scrolls to make room; Reset stats is
  now the last row, below the fold.
- **The breathing phases have sound again.** Each phase change plays a short
  sample with its own fade: a warm tone gliding up over a faint breath of air
  to breathe in, a soft bell to hold, the tone drawing back down to breathe
  out. They follow the Sound setting, Quiet Time and the
  watch's mute like every other cue. The tones were taken out after they
  restarted PebbleOS; a test on firmware 4.40 (`docs/phase-tone-test.md`) ran
  clean, and the `CAIRN_PHASE_TONES` build flag is gone. The samples come from
  `tools/gen_sfx.py` and are loaded when the session opens (on the Pebble 2
  Duo, short on memory, they share one buffer). The session screen draws in
  about a quarter of the time it used to (the scene cairn is drawn once and
  copied, the foam crest is filled instead of stroked), and pauses for the
  half-second a sound plays; a busy CPU made the sounds stutter and kept the
  speaker from taking the next one.
- **Follows the watch's speaker mute.** With the speaker muted in the watch
  settings, the cues stay silent (as in Quiet Time) and the Sound row in
  Settings reads "Muted" instead of a level that makes no sound.
- **The backlight breathes along (Pebble Time 2).** During a session the
  colour backlight follows the tide, warm sand when breathed out to sea blue
  when breathed in; pausing, stopping or finishing hands it back to the
  watch's own colour.
- **Streak in the launcher.** Cairn's line in the app list shows the running
  streak ("Streak: 5 d") and disappears at the moment the streak would break.
- **Pebble Round 2 (`gabbro`) and Pebble 2 Duo (`flint`).** On the round screen
  every screen keeps its 228px layout, centred in the circle; the tables and
  lists (progress, settings, result) sit in a 200px frame with their titles
  centred, the session's progress runs as a ring around the bezel, and the
  pause and stop panels pull in from the curved sides. The 144x168 screen gets
  its own row positions and smaller fonts on every screen, and on
  black-and-white everything that is read turns white while panels become black
  behind a white outline (dark grey would dither under the text).
- Settings page moved from `pebble-clay` to `@rebble/clay` 1.1.

## 1.2.0

- **Touch navigation, done in the app.** On the home screen a tap starts a
  session and a vertical flick changes the breathing pattern; the history list
  scrolls with your finger and keeps gliding when you let go; the settings list
  is tappable row by row; and a left-to-right swipe goes back from history,
  settings, insights and the result screen — and from the home screen it leaves
  the app, as BACK does. The session screen deliberately
  ignores touch, so a stray hand mid-exercise cannot interrupt the breathing.
- **The 1.1.0 opt-in into the watch's touch navigation is gone.** On firmware
  4.33.1 `app_touch_navigation_enable(true)` faults inside firmware on the first
  touch and kills the app, whatever the app does. Touch now comes straight from
  the raw touch stream (`src/c/touch_nav.c`), which bypasses that bridge.
  Reported as [PebbleOS#1865](https://github.com/coredevices/PebbleOS/issues/1865).

## 1.1.0

- **Real heart rate variability.** With SDK 4.33's HRV support, the calm
  score and the per-session coaching line now factor in RMSSD (the
  variability between successive heartbeats) when the watch's sensor
  delivers it, on top of completion, heart-rate drop and breath-linked
  swing. Without an HRV reading, the score falls back to exactly what it
  did before.
- **Touch on the history menu** now uses the watch's own touch navigation
  (SDK 4.33) instead of doing nothing — tap and scroll work like every
  other menu on the watch.

## 1.0.0

- Initial release: **Cairn**, a guided breathing coach with live heart rate for
  the Pebble Time 2. The breath is a tide — the waterline rises as you inhale
  and falls as you exhale, uncovering a cairn of stones on the shore.
- **Patterns:** Coherent (5.5 s in / 5.5 s out), Box (4-4-4-4), 4-7-8, and a
  Custom rhythm adjustable in half-second steps on the watch or from the phone.
  Session lengths of 1, 2, 3, 5, 10 or 15 minutes.
- **Guidance:** phase name and countdown above the water, optional haptic pulses
  on every phase change, and optional sound at four volume levels.
- **Sound:** soft sine phrases rather than beeps — a chime when the app opens, a
  resolve when a session ends, and a sample as you step through the volume
  levels. Per-note velocities ramp up and back down, which takes the edge off a
  note that would otherwise start and stop at full volume.
- **The breathing phases are haptic only.** Marking them with the speaker as
  well froze the app and restarted PebbleOS on real hardware, both as streamed
  PCM and as plain notes; every speaker call that survives is made from an event
  handler, and the phase machine runs on a timer. The vibration patterns (a
  nudge in, a double tick to hold, a longer pull out) carry the rhythm instead.
- **Heart rate:** the sensor is asked for a reading every second while the app
  is open, and a short priming screen at the start waits for a first reading so
  the opening heart rate is a real baseline. SELECT skips it.
- **Calm score** out of 100, mixing completion (35%), the drop in heart rate
  (35%) and how strongly the heart rate swings with the breath (30%,
  respiratory sinus arrhythmia). Deliberately not called HRV: the watch exposes
  only BPM, never beat-to-beat intervals. Without a reading the score falls back
  to completion alone.
- **Progression:** a cairn that grows with your daily streak, six badges (First
  breath, Steady, Zen week, Deep calm, Century, Tide), a personal best, and a
  history of the last 20 sessions.
- **Progress screen** with 7- and 30-day trends for calm score and resting heart
  rate plus one coaching line, computed from history kept on the phone.
- **Settings** on the watch and in a Clay page on the phone: pattern, custom
  rhythm, duration, sound volume, vibration, backlight, show heart rate, and a
  reset that clears the watch stats and the phone-side history together.
- **Five languages** — English, Nederlands, Français, Deutsch, Español —
  following the watch's system language.
