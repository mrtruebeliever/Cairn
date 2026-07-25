// Checks the phone-side trend logic in src/pkjs/index.js against the same
// scenarios as tools/test/test_logic.c, so the two implementations can't drift.
//
// Run: node tools/test/test_pkjs.js
//
// index.js is loaded with stubs for the Pebble runtime (require('pebble-clay'),
// Pebble, localStorage), and the functions under test are pulled out of the
// module scope by evaluating it in a sandbox.

const fs = require('fs');
const path = require('path');
const vm = require('vm');

const SRC = path.join(__dirname, '..', '..', 'src', 'pkjs', 'index.js');

let fail = 0;
function check(cond, label) {
  if (!cond) {
    console.log('  FAIL ' + label);
    fail++;
  }
}
function checkEq(actual, expect, label) {
  if (actual !== expect) {
    console.log('  FAIL ' + label + ': got ' + actual + ', want ' + expect);
    fail++;
  }
}

// TrendTip, must match src/c/config.h.
const TT = {
  SCORE_UP: 0, SCORE_DOWN: 1, PLATEAU: 2, HR_DOWN: 3, HR_UP: 4,
  STREAK_UP: 5, STREAK_BROKEN: 6, FEW: 7, QUITS: 8, PRESET_BEST: 9,
};

function loadModule() {
  const store = {};
  const sandbox = {
    console: {log: function () {}},
    localStorage: {
      getItem: function (k) { return k in store ? store[k] : null; },
      setItem: function (k, v) { store[k] = String(v); },
      removeItem: function (k) { delete store[k]; },
    },
    Pebble: {
      addEventListener: function () {},
      sendAppMessage: function (msg, ok) { sandbox.lastMessage = msg; if (ok) ok(); },
    },
    require: function (name) {
      if (name === 'pebble-clay') {
        return function Clay() { this.getSettings = function () { return {}; }; };
      }
      if (name === './config.json') return [];
      throw new Error('unexpected require: ' + name);
    },
    lastMessage: null,
  };
  sandbox.global = sandbox;
  const ctx = vm.createContext(sandbox);
  // Expose the internals the tests need without editing index.js itself.
  const code = fs.readFileSync(SRC, 'utf8') +
    '\n;this.__api = {computeTrend, recordSession, resetHistory, computeStreak, ' +
    'dayNumber, loadHistory, saveHistory, bestPreset, HISTORY_CAP};';
  vm.runInContext(code, ctx, {filename: 'index.js'});
  return {api: sandbox.__api, store: store, sandbox: sandbox};
}

function ymdOffset(daysAgo) {
  const d = new Date(Date.now() - daysAgo * 86400000);
  return d.getFullYear() * 10000 + (d.getMonth() + 1) * 100 + d.getDate();
}

function session(daysAgo, score, hr, completion, preset) {
  return {
    date: ymdOffset(daysAgo),
    dur: 180,
    hrStart: hr,
    hrEnd: hr ? hr - 4 : 0,
    score: score,
    preset: preset,
    completion: completion,
  };
}

function withHistory(list) {
  const m = loadModule();
  m.store['cairn_history'] = JSON.stringify(list);
  return m;
}

// --- date arithmetic --------------------------------------------------------
console.log('dayNumber');
{
  const m = loadModule();
  const d = m.api.dayNumber;
  checkEq(d(20260301) - d(20260228), 1, 'Feb 28 -> Mar 1 2026');
  checkEq(d(20240301) - d(20240229), 1, 'leap day 2024');
  checkEq(d(20250101) - d(20241231), 1, 'year boundary');
  checkEq(d(20260801) - d(20260701), 31, 'July has 31 days');
}

// --- streak ----------------------------------------------------------------
console.log('computeStreak');
{
  const m = loadModule();
  const s = m.api.computeStreak;
  checkEq(s([]), 0, 'no sessions');
  checkEq(s([session(0, 70, 70, 100, 0)]), 1, 'today only');
  checkEq(s([session(1, 70, 70, 100, 0)]), 1, 'yesterday only still counts');
  checkEq(s([session(2, 70, 70, 100, 0)]), 0, 'two days ago is broken');
  checkEq(s([session(2, 70, 70, 100, 0), session(1, 70, 70, 100, 0),
             session(0, 70, 70, 100, 0)]), 3, 'three consecutive days');
  checkEq(s([session(3, 70, 70, 100, 0), session(1, 70, 70, 100, 0),
             session(0, 70, 70, 100, 0)]), 2, 'a gap stops the count');
  checkEq(s([session(0, 70, 70, 100, 0), session(0, 71, 70, 100, 0)]), 1,
          'two sessions on one day count once');
}

// --- trends: same scenarios as the C tests ---------------------------------
console.log('computeTrend');
{
  const m = withHistory([
    session(10, 50, 80, 100, 0), session(9, 52, 80, 100, 0),
    session(3, 75, 72, 100, 0), session(2, 79, 72, 100, 0),
    session(1, 77, 72, 100, 0),
  ]);
  const t = m.api.computeTrend();
  checkEq(t.TREND_SCORE_7, 77, '7-day average');
  checkEq(t.TREND_TIP, TT.SCORE_UP, 'rising score is the headline');
  // TREND_DIR packs (score + 1) * 10 + (hr + 1): rising score, falling HR.
  checkEq(t.TREND_DIR, 20, 'packed direction: score up, HR down');
  // Sessions on days 1, 2 and 3 ago: the streak runs back from yesterday.
  checkEq(t.TREND_STREAK, 3, 'streak counted back from yesterday');
}
{
  const m = withHistory([session(1, 70, 70, 100, 0)]);
  checkEq(m.api.computeTrend().TREND_TIP, TT.FEW, 'quiet week');
}
{
  const m = withHistory([
    session(3, 40, 70, 30, 0), session(2, 45, 70, 40, 0), session(1, 50, 70, 100, 0),
  ]);
  checkEq(m.api.computeTrend().TREND_TIP, TT.QUITS, 'two of three cut short');
}
{
  const m = withHistory([
    session(4, 70, 70, 100, 0), session(3, 72, 70, 100, 0), session(2, 74, 70, 100, 0),
  ]);
  const t = m.api.computeTrend();
  checkEq(t.TREND_STREAK, 0, 'nothing yesterday or today');
  checkEq(t.TREND_TIP, TT.STREAK_BROKEN, 'broken streak is the headline');
}
{
  const m = withHistory([
    session(5, 90, 0, 100, 1), session(4, 92, 0, 100, 1),
    session(3, 60, 0, 100, 2), session(2, 62, 0, 100, 2),
  ]);
  checkEq(m.api.computeTrend().TREND_PRESET, 1, 'Box scores clearly highest');
}
{
  const m = withHistory([
    session(5, 80, 0, 100, 1), session(4, 81, 0, 100, 1),
    session(3, 79, 0, 100, 2), session(2, 80, 0, 100, 2),
  ]);
  checkEq(m.api.computeTrend().TREND_PRESET, -1, 'a one-point lead is no recommendation');
}
{
  const m = loadModule();
  const t = m.api.computeTrend();
  checkEq(t.TREND_SCORE_7, -1, 'no data');
  checkEq(t.TREND_TIP, -1, 'no tip without data');
}

// --- history storage -------------------------------------------------------
console.log('history');
{
  const m = loadModule();
  const rec = {
    ES_DATE: ymdOffset(0), ES_DUR: 180, ES_HRSTART: 72, ES_HREND: 66,
    ES_SCORE: 81, ES_PRESET: 0, ES_COMPLETION: 100,
  };
  m.api.recordSession(rec);
  checkEq(m.api.loadHistory().length, 1, 'session stored');
  m.api.recordSession(rec);
  checkEq(m.api.loadHistory().length, 1, 'duplicate delivery ignored');
  m.api.recordSession(Object.assign({}, rec, {ES_SCORE: 84}));
  checkEq(m.api.loadHistory().length, 2, 'a different session is appended');
  check(m.sandbox.lastMessage && 'TREND_SCORE_7' in m.sandbox.lastMessage,
        'storing a session sends fresh trends back');
}
{
  // The cap trims the oldest entries, never the newest.
  const m = loadModule();
  const big = [];
  for (let i = 0; i < m.api.HISTORY_CAP + 20; i++) big.push(session(0, i % 100, 70, 100, 0));
  const saved = m.api.saveHistory(big);
  checkEq(saved.length, m.api.HISTORY_CAP, 'history capped at HISTORY_CAP');
  checkEq(saved[saved.length - 1].score, big[big.length - 1].score, 'newest kept');
}
{
  // Corrupt storage must not throw.
  const m = loadModule();
  m.store['cairn_history'] = 'not json at all';
  checkEq(m.api.loadHistory().length, 0, 'unparseable history recovers to empty');
  m.store['cairn_history'] = '{"not":"an array"}';
  checkEq(m.api.loadHistory().length, 0, 'non-array history recovers to empty');
}

// --- reset from the watch ---------------------------------------------------
console.log('resetHistory');
{
  const m = loadModule();
  m.api.recordSession({
    ES_DATE: ymdOffset(0), ES_DUR: 180, ES_HRSTART: 72, ES_HREND: 66,
    ES_SCORE: 81, ES_PRESET: 0, ES_COMPLETION: 100,
  });
  checkEq(m.api.loadHistory().length, 1, 'one session before the reset');
  m.api.resetHistory();
  checkEq(m.api.loadHistory().length, 0, 'reset clears the phone history');
  checkEq(m.sandbox.lastMessage.TREND_SCORE_7, -1,
          'reset answers with an empty trend, not the deleted one');
}

if (fail) {
  console.log('\n' + fail + ' check(s) FAILED');
  process.exit(1);
}
console.log('\nall checks passed');
