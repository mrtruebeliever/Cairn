var Clay = require('@rebble/clay');
var clayConfig = require('./config.json');
var clay = new Clay(clayConfig);

// The phone keeps the long history the watch has no room for (the watch stores
// only the last 20 sessions) and computes the 7/30-day trends from it. The
// analysis mirrors insights_local_trend() in src/c/insights.c, including the
// trend-tip priority order — TREND_TIP is an index into the TrendTip enum in
// src/c/config.h, so the order here is part of the wire format.
var HISTORY_KEY = 'cairn_history';
var HISTORY_CAP = 180;

// TrendTip, must match src/c/config.h.
var TT_SCORE_UP = 0, TT_SCORE_DOWN = 1, TT_PLATEAU = 2, TT_HR_DOWN = 3,
    TT_HR_UP = 4, TT_STREAK_UP = 5, TT_STREAK_BROKEN = 6, TT_FEW = 7,
    TT_QUITS = 8, TT_PRESET_BEST = 9;

var NUM_PRESETS = 4;

function loadHistory() {
  try {
    var raw = localStorage.getItem(HISTORY_KEY);
    var list = raw ? JSON.parse(raw) : [];
    return Array.isArray(list) ? list : [];
  } catch (e) {
    console.log('cairn: history unreadable, starting over (' + e + ')');
    return [];
  }
}

function saveHistory(list) {
  if (list.length > HISTORY_CAP) list = list.slice(list.length - HISTORY_CAP);
  try {
    localStorage.setItem(HISTORY_KEY, JSON.stringify(list));
  } catch (e) {
    console.log('cairn: could not save history (' + e + ')');
  }
  return list;
}

// yyyymmdd -> days since epoch, so dates can be subtracted across months.
function dayNumber(ymd) {
  var y = Math.floor(ymd / 10000);
  var m = Math.floor(ymd / 100) % 100;
  var d = ymd % 100;
  return Math.floor(Date.UTC(y, m - 1, d) / 86400000);
}

function todayYmd() {
  var t = new Date();
  return t.getFullYear() * 10000 + (t.getMonth() + 1) * 100 + t.getDate();
}

function average(values) {
  if (!values.length) return -1;
  var sum = 0;
  for (var i = 0; i < values.length; i++) sum += values[i];
  return Math.round(sum / values.length);
}

// A direction with a deadband, so day-to-day noise doesn't read as progress.
function direction(now, prev, deadband) {
  if (now < 0 || prev < 0) return 0;
  if (now - prev > deadband) return 1;
  if (prev - now > deadband) return -1;
  return 0;
}

// Consecutive days with at least one session, counted back from today (or from
// yesterday — a streak isn't broken until a whole day has been skipped).
function computeStreak(list) {
  var days = {};
  for (var i = 0; i < list.length; i++) days[dayNumber(list[i].date)] = true;
  var today = dayNumber(todayYmd());
  var cursor = days[today] ? today : (days[today - 1] ? today - 1 : null);
  if (cursor === null) return 0;
  var streak = 0;
  while (days[cursor]) {
    streak++;
    cursor--;
  }
  return streak;
}

function bucket(list, minAge, maxAge, today) {
  var out = {scores: [], hrs: [], quits: 0, sessions: 0};
  for (var i = 0; i < list.length; i++) {
    var r = list[i];
    var age = today - dayNumber(r.date);
    if (age < minAge || age >= maxAge) continue;
    out.sessions++;
    out.scores.push(r.score);
    if (r.hrStart) out.hrs.push(r.hrStart);
    if (typeof r.completion === 'number' && r.completion < 60) out.quits++;
  }
  return out;
}

// The preset the user scores highest with — at least two sessions on it and a
// five-point lead, otherwise there is nothing worth recommending.
function bestPreset(list) {
  var sums = [], counts = [];
  var p;
  for (p = 0; p < NUM_PRESETS; p++) { sums[p] = 0; counts[p] = 0; }
  for (var i = 0; i < list.length; i++) {
    p = list[i].preset;
    if (p >= 0 && p < NUM_PRESETS) { sums[p] += list[i].score; counts[p]++; }
  }
  var best = -1, bestAvg = -1, runnerAvg = -1;
  for (p = 0; p < NUM_PRESETS; p++) {
    if (counts[p] < 2) continue;
    var avg = sums[p] / counts[p];
    if (avg > bestAvg) { runnerAvg = bestAvg; best = p; bestAvg = avg; }
    else if (avg > runnerAvg) { runnerAvg = avg; }
  }
  if (best < 0) return -1;
  if (runnerAvg >= 0 && bestAvg - runnerAvg < 5) return -1;
  return best;
}

function computeTrend() {
  var list = loadHistory();
  var today = dayNumber(todayYmd());

  var b7 = bucket(list, 0, 7, today);
  var b14 = bucket(list, 7, 14, today);
  var b30 = bucket(list, 0, 30, today);

  var score7 = average(b7.scores), score30 = average(b30.scores);
  var hr7 = average(b7.hrs), hr30 = average(b30.hrs);
  var dirScore = direction(score7, average(b14.scores), 3);
  var dirHr = direction(hr7, average(b14.hrs), 2);
  var streak = computeStreak(list);
  var preset = bestPreset(list);

  // Same rule order as insights_local_trend(): most actionable advice first.
  var tip = -1;
  if (list.length > 0 && streak === 0) tip = TT_STREAK_BROKEN;
  else if (b7.sessions > 0 && b7.sessions < 3) tip = TT_FEW;
  else if (b7.sessions >= 2 && b7.quits * 2 >= b7.sessions) tip = TT_QUITS;
  else if (dirScore > 0) tip = TT_SCORE_UP;
  else if (dirScore < 0) tip = TT_SCORE_DOWN;
  else if (dirHr < 0) tip = TT_HR_DOWN;
  else if (dirHr > 0) tip = TT_HR_UP;
  else if (streak >= 3) tip = TT_STREAK_UP;
  else if (preset >= 0) tip = TT_PRESET_BEST;
  else if (score7 >= 0) tip = TT_PLATEAU;

  return {
    TREND_SCORE_7: score7,
    TREND_SCORE_30: score30,
    TREND_HR_7: hr7,
    TREND_HR_30: hr30,
    TREND_STREAK: streak,
    // Both directions in one tuple: (score + 1) * 10 + (hr + 1).
    TREND_DIR: (dirScore + 1) * 10 + (dirHr + 1),
    TREND_TIP: tip,
    TREND_PRESET: preset
  };
}

function sendTrend() {
  var msg = computeTrend();
  Pebble.sendAppMessage(msg, function () {
    console.log('cairn: trend sent (score7=' + msg.TREND_SCORE_7 +
                ', tip=' + msg.TREND_TIP + ')');
  }, function (e) {
    console.log('cairn: trend send failed (' + JSON.stringify(e) + ')');
  });
}

function recordSession(dict) {
  var list = loadHistory();
  var entry = {
    date: dict.ES_DATE,
    dur: dict.ES_DUR || 0,
    hrStart: dict.ES_HRSTART || 0,
    hrEnd: dict.ES_HREND || 0,
    score: dict.ES_SCORE || 0,
    preset: typeof dict.ES_PRESET === 'number' ? dict.ES_PRESET : 0,
    completion: typeof dict.ES_COMPLETION === 'number' ? dict.ES_COMPLETION : 100
  };
  // Guard against a duplicate delivery of the same record.
  var last = list[list.length - 1];
  if (last && last.date === entry.date && last.dur === entry.dur &&
      last.score === entry.score) {
    console.log('cairn: duplicate session ignored');
  } else {
    list.push(entry);
    list = saveHistory(list);
  }
  console.log('cairn: session stored (' + list.length + ' total)');
  sendTrend();
}

// "Reset stats" on the watch clears the watch's own 20 records; without this the
// phone would keep answering with trends drawn from the history the user just
// deleted, which is exactly what they did not ask for.
function resetHistory() {
  try {
    localStorage.removeItem(HISTORY_KEY);
  } catch (e) {
    console.log('cairn: could not clear history (' + e + ')');
  }
  console.log('cairn: history cleared on request from the watch');
  sendTrend();
}

Pebble.addEventListener('appmessage', function (e) {
  var dict = e.payload || {};
  if (typeof dict.ES_DATE === 'number' && dict.ES_DATE > 0) {
    recordSession(dict);
  } else if (dict.RESET_ALL) {
    resetHistory();
  } else if (dict.TREND_REQ) {
    sendTrend();
  }
});

Pebble.addEventListener('ready', function () {
  console.log('cairn: pkjs ready');
});

// Clay handles showConfiguration itself; webviewclosed only has to coerce the
// select strings to numbers and convert the custom rhythm from whole seconds
// (what the sliders offer) to the tenths the watch stores.
Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) return;
  var settings = clay.getSettings(e.response, false);
  var out = {};

  function num(key) {
    if (settings[key] === undefined) return null;
    var v = settings[key];
    if (v && typeof v === 'object' && 'value' in v) v = v.value;
    var n = Number(v);
    return isNaN(n) ? null : n;
  }

  function bool(key) {
    if (settings[key] === undefined) return null;
    var v = settings[key];
    if (v && typeof v === 'object' && 'value' in v) v = v.value;
    return v ? 1 : 0;
  }

  var simple = ['SETTINGS_PRESET', 'SESSION_LEN', 'SOUND_VOL', 'REMINDER'];
  for (var i = 0; i < simple.length; i++) {
    var n = num(simple[i]);
    if (n !== null) out[simple[i]] = n;
  }
  var tenths = ['CUSTOM_IN', 'CUSTOM_HOLD1', 'CUSTOM_OUT', 'CUSTOM_HOLD2'];
  for (i = 0; i < tenths.length; i++) {
    var t = num(tenths[i]);
    if (t !== null) out[tenths[i]] = Math.round(t * 10);
  }
  var flags = ['HAPTICS_ON', 'SHOW_HR'];
  for (i = 0; i < flags.length; i++) {
    var b = bool(flags[i]);
    if (b !== null) out[flags[i]] = b;
  }

  Pebble.sendAppMessage(out, function () {
    console.log('cairn: settings sent');
  }, function (err) {
    console.log('cairn: settings send failed (' + JSON.stringify(err) + ')');
  });
});
