# Changelog

## 1.0.0

- Initial release: **Cairn**, a guided breathing coach with live heart rate for
  the Pebble Time 2. The breath is a tide — the waterline rises as you inhale
  and falls as you exhale, uncovering a cairn of stones on the shore.
- **Patterns:** Coherent (5.5 s in / 5.5 s out), Box (4-4-4-4), 4-7-8, and a
  Custom rhythm adjustable in half-second steps on the watch or from the phone.
  Session lengths of 1, 2, 3, 5, 10 or 15 minutes.
- **Guidance:** phase name and countdown above the water, optional haptic pulses
  on every phase change, and optional sound at four volume levels.
- **Generated audio.** Every cue is synthesised as raw PCM rather than played as
  notes, because a note restarts its waveform at full amplitude and that click
  is what makes a short beep sound harsh. Filtered noise makes the surf: a wave
  breaks when the app opens, a swell rises with the inhale and draws back on the
  exhale, and the chimes are struck bells with a real decay. Nothing is stored —
  it costs no resources — and `tools/test/preview_sfx.sh` renders the lot to WAV
  for listening on a PC, since the emulator has no speaker.
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
