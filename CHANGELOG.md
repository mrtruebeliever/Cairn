# Changelog

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
